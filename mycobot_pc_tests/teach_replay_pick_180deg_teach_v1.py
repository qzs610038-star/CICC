#!/usr/bin/env python3
"""myCobot 280 180°点位示教与受控诊断脚本 v1.

本脚本是点位候选生成器，不是比赛自动抓放程序：

* --teach-only 只做五点人工示教、保存 JSON，然后释放舵机；
* --probe-* 只做显式指定的单段/短段关节角诊断，不碰夹爪；
* --check-preset 只做离线点位与外部证据校验，不连接串口；
* 不使用笛卡尔坐标运动命令，不包含异步回零、软到位自动放行或连续抓取。

JSON 通过离线外部证据校验后，才可作为后续 arm_positions_180deg_v1.c/.h
的候选来源；PC 结果不等于板上或比赛闭环已验证。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
import time
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable


ROOT = Path(__file__).resolve().parents[1]
PRESETS_DIR = ROOT / "mycobot_pc_tests" / "presets"
AUDIT_DIR = ROOT / "mycobot_pc_tests" / "audit_logs"

JOINT_COUNT = 6
BAUD = 1_000_000
R_MAX_MM = 280.0
R_RECOMMENDED_MM = 250.0
HOME_ANGLES = [0.0] * JOINT_COUNT

# 只作为首次受控诊断的保守起点；正式点位必须记录实际确认的速度。
PROBE_LONG_SPEED = 15
PROBE_SHORT_SPEED = 12
PROBE_LONG_TIMEOUT = 20
PROBE_SHORT_TIMEOUT = 10

ANGLE_READ_TOL_DEG = 0.8
COORD_READ_TOL_MM = 2.0
STRICT_ANGLE_TOL_DEG = 2.0
SHORT_ARM_JOINT_MAX_DELTA_DEG = 30.0
SHORT_WRIST6_MAX_DELTA_DEG = 45.0
RETURN_ARM_JOINT_MAX_DELTA_DEG = 90.0
RETURN_WRIST6_MAX_DELTA_DEG = 120.0
HOME_READY_TARGET_ARM_MAX_DEG = 40.0
HOME_READY_HARD_ARM_MAX_DEG = 45.0

POINT_NAMES = ("pick_hover", "pick", "drop_hover", "drop", "home_ready")


class TeachAbort(RuntimeError):
    """用户在示教阶段主动放弃。"""


class OutputMirror:
    def __init__(self, console: Any, file_obj: Any) -> None:
        self.console = console
        self.file_obj = file_obj

    def write(self, text: str) -> int:
        self.console.write(text)
        self.file_obj.write(text)
        return len(text)

    def flush(self) -> None:
        self.console.flush()
        self.file_obj.flush()


@contextmanager
def capture_log(path: Path | None) -> Iterable[None]:
    if path is None:
        yield
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as log_file:
        old_stdout = sys.stdout
        sys.stdout = OutputMirror(old_stdout, log_file)
        try:
            print(f"[audit] log={path}")
            yield
        finally:
            sys.stdout = old_stdout


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def script_sha256() -> str:
    return hashlib.sha256(Path(__file__).read_bytes()).hexdigest()


def finite_numbers(values: Any, length: int) -> bool:
    return (
        isinstance(values, list)
        and len(values) >= length
        and all(isinstance(v, (int, float)) and math.isfinite(float(v)) for v in values[:length])
    )


def valid_angles(values: Any) -> bool:
    return finite_numbers(values, JOINT_COUNT) and all(-180.0 <= float(v) <= 180.0 for v in values[:JOINT_COUNT])


def valid_coords(values: Any) -> bool:
    return finite_numbers(values, 3) and math.hypot(float(values[0]), float(values[1])) <= R_MAX_MM


def radius(coords: list[float]) -> float:
    return math.hypot(float(coords[0]), float(coords[1]))


def max_delta(a: list[float], b: list[float], count: int = JOINT_COUNT) -> float:
    return max(abs(float(x) - float(y)) for x, y in zip(a[:count], b[:count]))


def load_preset_path(value: str) -> Path:
    candidate = Path(value)
    if candidate.is_file():
        return candidate
    if candidate.suffix.lower() == ".json":
        candidate = PRESETS_DIR / candidate.name
    else:
        candidate = PRESETS_DIR / f"teach_points_{value}.json"
    return candidate


def save_preset_path(name: str) -> Path:
    candidate = Path(name)
    if candidate.suffix.lower() == ".json":
        return candidate if candidate.parent != Path(".") else PRESETS_DIR / candidate.name
    return PRESETS_DIR / f"teach_points_{name}.json"


def read_stable(read_fn: Callable[[], Any], validator: Callable[[Any], bool],
                tolerance: float, label: str, attempts: int = 8) -> list[float]:
    previous: list[float] | None = None
    stable = 0
    for _ in range(attempts):
        value = read_fn()
        if validator(value):
            current = [float(v) for v in value]
            if previous is not None and max_delta(current, previous, len(current)) <= tolerance:
                stable += 1
                if stable >= 2:
                    return current
            else:
                stable = 1
            previous = current
        time.sleep(0.10)
    raise RuntimeError(f"{label} 未获得连续稳定读数")


def read_pose(mc: Any) -> tuple[list[float], list[float]]:
    angles = read_stable(mc.get_angles, valid_angles, ANGLE_READ_TOL_DEG, "angles")[:JOINT_COUNT]
    coords = read_stable(mc.get_coords, valid_coords, COORD_READ_TOL_MM, "coords")
    return angles, coords


def point(name: str, angles: list[float], coords: list[float], is_hover: bool,
          speed: int | None = None) -> dict[str, Any]:
    return {
        "name": name,
        "angles": [round(float(v), 3) for v in angles[:JOINT_COUNT]],
        "coords": [round(float(v), 3) for v in coords],
        "is_hover": bool(is_hover),
        "recorded_at": utc_now(),
        "confirmed_speed": speed,
    }


def check_short_pair(src: dict[str, Any], dst: dict[str, Any], label: str) -> None:
    arm_delta = max_delta(src["angles"], dst["angles"], 5)
    wrist_delta = abs(float(src["angles"][5]) - float(dst["angles"][5]))
    if arm_delta > SHORT_ARM_JOINT_MAX_DELTA_DEG:
        raise ValueError(f"{label}: 1-5轴最大差 {arm_delta:.2f}° > {SHORT_ARM_JOINT_MAX_DELTA_DEG}°")
    if wrist_delta > SHORT_WRIST6_MAX_DELTA_DEG:
        raise ValueError(f"{label}: J6差 {wrist_delta:.2f}° > {SHORT_WRIST6_MAX_DELTA_DEG}°")


def check_return_pair(src: dict[str, Any], dst: dict[str, Any], label: str) -> None:
    arm_delta = max_delta(src["angles"], dst["angles"], 5)
    wrist_delta = abs(float(src["angles"][5]) - float(dst["angles"][5]))
    if arm_delta > RETURN_ARM_JOINT_MAX_DELTA_DEG:
        raise ValueError(f"{label}: 1-5轴最大差 {arm_delta:.2f}° > {RETURN_ARM_JOINT_MAX_DELTA_DEG}°")
    if wrist_delta > RETURN_WRIST6_MAX_DELTA_DEG:
        raise ValueError(f"{label}: J6差 {wrist_delta:.2f}° > {RETURN_WRIST6_MAX_DELTA_DEG}°")


def home_ready_arm_max(point_data: dict[str, Any]) -> float:
    return max_delta(point_data["angles"], HOME_ANGLES, 5)


def validate_external_reference(data: dict[str, Any]) -> None:
    ext = data.get("external_reference")
    if not isinstance(ext, dict) or ext.get("complete") is not True:
        raise ValueError("external_reference.complete 未为 true，仍是候选点位")
    required_text = ("base_marker_before", "base_marker_after", "photo_or_video", "mount_revision")
    for key in required_text:
        if not isinstance(ext.get(key), str) or not ext[key].strip():
            raise ValueError(f"external_reference.{key} 缺失")
    rotation = ext.get("drop_rotation_deg")
    if not isinstance(rotation, (int, float)) or not 170.0 <= float(rotation) <= 190.0:
        raise ValueError("drop_rotation_deg 必须在 170°..190°")
    drop_radius = ext.get("drop_radius_mm")
    if not isinstance(drop_radius, (int, float)) or not 0.0 < float(drop_radius) <= R_MAX_MM:
        raise ValueError("drop_radius_mm 必须在 0..280mm")
    if ext.get("max_reach_verified") is not True:
        raise ValueError("max_reach_verified 未确认")
    base_shift = ext.get("base_shift_mm")
    if not isinstance(base_shift, (int, float)) or float(base_shift) > 0.5:
        raise ValueError("base_shift_mm 缺失或超过 0.5mm")


def annotate_preset(source_name: str, external_file: str, output_name: str) -> Path:
    source_path = load_preset_path(source_name)
    if not source_path.is_file():
        raise FileNotFoundError(f"点位文件不存在: {source_path}")
    data = json.loads(source_path.read_text(encoding="utf-8"))
    external_data = json.loads(Path(external_file).read_text(encoding="utf-8"))
    if isinstance(external_data, dict) and isinstance(external_data.get("external_reference"), dict):
        external_data = external_data["external_reference"]
    if not isinstance(external_data, dict):
        raise ValueError("外部证据文件必须是 JSON 对象或包含 external_reference 对象")
    data["external_reference"] = external_data
    validate_preset(data, require_external=True)
    data["acceptance_status"] = "EXTERNAL_REFERENCE_VERIFIED_CANDIDATE"
    data["external_reference_attached_at"] = utc_now()
    output_path = save_preset_path(output_name)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] 外部证据已附加并通过离线校验: {output_path}")
    return output_path


def validate_preset(data: dict[str, Any], require_external: bool = False) -> list[str]:
    points = data.get("points")
    if not isinstance(points, dict) or any(name not in points for name in POINT_NAMES):
        raise ValueError(f"点位必须完整包含: {', '.join(POINT_NAMES)}")
    for name in POINT_NAMES:
        item = points[name]
        if not valid_angles(item.get("angles")):
            raise ValueError(f"{name}.angles 无效")
        if not valid_coords(item.get("coords")):
            raise ValueError(f"{name}.coords 无效或超出 R_MAX")
    check_short_pair(points["pick_hover"], points["pick"], "pick_hover->pick")
    check_short_pair(points["pick"], points["pick_hover"], "pick->pick_hover")
    check_short_pair(points["drop_hover"], points["drop"], "drop_hover->drop")
    check_short_pair(points["drop"], points["drop_hover"], "drop->drop_hover")
    check_return_pair(points["drop_hover"], points["home_ready"], "drop_hover->home_ready")
    hr = home_ready_arm_max(points["home_ready"])
    if hr > HOME_READY_HARD_ARM_MAX_DEG:
        raise ValueError(f"home_ready arm_max_diff={hr:.2f}° > {HOME_READY_HARD_ARM_MAX_DEG}°")
    warnings: list[str] = []
    if hr > HOME_READY_TARGET_ARM_MAX_DEG:
        warnings.append(f"home_ready arm_max_diff={hr:.2f}° 落在 40..45°警告区")
    if require_external:
        validate_external_reference(data)
    elif data.get("external_reference", {}).get("complete") is not True:
        warnings.append("external_reference 未完成，不能作为180°正式验收点位")
    return warnings


def default_external_reference() -> dict[str, Any]:
    return {
        "complete": False,
        "base_marker_before": "",
        "base_marker_after": "",
        "base_shift_mm": None,
        "base_rotation_deg": None,
        "drop_rotation_deg": None,
        "drop_rotation_tolerance_deg": 10.0,
        "drop_radius_mm": None,
        "max_reach_verified": False,
        "photo_or_video": "",
        "mount_revision": "",
    }


def build_preset(points: dict[str, dict[str, Any]], args: argparse.Namespace) -> dict[str, Any]:
    return {
        "schema": "mycobot_180deg_teach_v1",
        "name": args.save_preset,
        "created_at": utc_now(),
        "source_script": Path(__file__).name,
        "source_script_sha256": script_sha256(),
        "created_from_log": args.save_preset_log or "",
        "object_size_cm": args.save_preset_obj or "",
        "points": points,
        "safety": {
            "R_MAX_MM": R_MAX_MM,
            "R_RECOMMENDED_MM": R_RECOMMENDED_MM,
            "home_ready_arm_max_diff": round(home_ready_arm_max(points["home_ready"]), 3),
            "home_ready_target_arm_max_diff": HOME_READY_TARGET_ARM_MAX_DEG,
            "home_ready_hard_arm_max_diff": HOME_READY_HARD_ARM_MAX_DEG,
        },
        "external_reference": default_external_reference(),
        "acceptance_status": "CANDIDATE_UNVERIFIED",
        "notes": args.save_preset_notes or "",
    }


def record_teach_point(mc: Any, name: str, is_hover: bool) -> dict[str, Any]:
    while True:
        answer = input(f"-> 手动拖动到【{name}】，稳定后按 Enter 读取（q 放弃）: ")
        if answer.strip().lower() == "q":
            raise TeachAbort(f"用户放弃示教 {name}")
        try:
            angles, coords = read_pose(mc)
        except RuntimeError as exc:
            print(f"[重试] {exc}")
            continue
        print(f"{name} angles={angles}")
        print(f"{name} coords={coords}, R={radius(coords):.1f}mm")
        if radius(coords) > R_MAX_MM:
            print(f"[拦截] R>{R_MAX_MM}mm，请重新示教")
            continue
        if name == "home_ready":
            hr = max_delta(angles, HOME_ANGLES, 5)
            print(f"home_ready arm_max_diff={hr:.2f}°")
            if hr > HOME_READY_HARD_ARM_MAX_DEG:
                print("[拦截] home_ready 超过 45°硬门，请重新示教")
                continue
            if hr > HOME_READY_TARGET_ARM_MAX_DEG:
                print("[警告] home_ready 位于 40..45°余量不足区")
        confirm = input("确认保存？(y=保存/n=重来/q=放弃): ").strip().lower()
        if confirm == "q":
            raise TeachAbort(f"用户放弃保存 {name}")
        if confirm == "y":
            return point(name, angles, coords, is_hover)


def teach_only(mc: Any, args: argparse.Namespace) -> Path:
    print("[安全] 即将释放舵机；本模式不会 power_on、回零或自动抓放。")
    input("确认机械臂已支撑、急停/断电可用后按 Enter，Ctrl+C 放弃: ")
    mc.release_all_servos()
    points = {
        "pick_hover": record_teach_point(mc, "pick_hover", True),
        "pick": record_teach_point(mc, "pick", False),
        "drop_hover": record_teach_point(mc, "drop_hover", True),
        "drop": record_teach_point(mc, "drop", False),
        "home_ready": record_teach_point(mc, "home_ready", True),
    }
    data = build_preset(points, args)
    warnings = validate_preset(data)
    path = save_preset_path(args.save_preset)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] 候选点位已保存: {path}")
    for warning in warnings:
        print(f"[WARN] {warning}")
    print("[安全] 当前仍是 CANDIDATE_UNVERIFIED，不得直接生成/启用板上动作点表。")
    return path


def load_and_validate(name: str, require_external: bool = False) -> tuple[Path, dict[str, Any]]:
    path = load_preset_path(name)
    if not path.is_file():
        raise FileNotFoundError(f"点位文件不存在: {path}")
    data = json.loads(path.read_text(encoding="utf-8"))
    warnings = validate_preset(data, require_external=require_external)
    print(f"[PASS] 点位结构校验: {path}")
    for warning in warnings:
        print(f"[WARN] {warning}")
    return path, data


def angles_from_robot(mc: Any) -> list[float]:
    values = mc.get_angles()
    if not valid_angles(values):
        raise RuntimeError(f"实际角度读取失败: {values}")
    return [float(v) for v in values[:JOINT_COUNT]]


def move_strict(mc: Any, target: list[float], speed: int, timeout: int, label: str,
                expected_coords: list[float] | None = None) -> None:
    print(f"[MOVE] {label}: joint angles, speed={speed}, timeout={timeout}")
    result = mc.sync_send_angles(target, speed, timeout=timeout)
    actual = angles_from_robot(mc)
    diff = max_delta(actual, target)
    print(f"[READBACK] {label}: result={result}, max_angle_diff={diff:.2f}°")
    if expected_coords is not None:
        actual_coords = read_stable(mc.get_coords, valid_coords, COORD_READ_TOL_MM, "coords")
        xyz_diff = math.sqrt(sum((float(actual_coords[i]) - float(expected_coords[i])) ** 2 for i in range(3)))
        print(f"[READBACK] {label}: target_coords={expected_coords[:3]}, actual_coords={actual_coords[:3]}, xyz_diff={xyz_diff:.2f}mm")
    if result != 1 or diff > STRICT_ANGLE_TOL_DEG:
        raise RuntimeError(f"{label} 未严格到位；禁止继续，result={result}, diff={diff:.2f}°")


def prepare_home(mc: Any) -> None:
    mc.power_on()
    current = angles_from_robot(mc)
    if max_delta(current, HOME_ANGLES, 5) > HOME_READY_HARD_ARM_MAX_DEG:
        raise RuntimeError("当前姿态离 HOME 超过45°；禁止脚本自动回零，请现场人工扶稳后重试")
    move_strict(mc, HOME_ANGLES, PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "HOME")


def probe(mc: Any, data: dict[str, Any], mode: str) -> None:
    points = data["points"]
    prepare_home(mc)
    print("[安全] 探针模式不发送任何夹爪命令，保持当前夹爪状态。")
    if mode == "probe-pick-hover":
        move_strict(mc, points["pick_hover"]["angles"], PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "pick_hover", points["pick_hover"]["coords"])
        move_strict(mc, HOME_ANGLES, PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "HOME-return")
    elif mode == "probe-drop-hover":
        move_strict(mc, points["drop_hover"]["angles"], PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "drop_hover", points["drop_hover"]["coords"])
        move_strict(mc, HOME_ANGLES, PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "HOME-return")
    elif mode == "probe-pick":
        move_strict(mc, points["pick_hover"]["angles"], PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "pick_hover", points["pick_hover"]["coords"])
        move_strict(mc, points["pick"]["angles"], PROBE_SHORT_SPEED, PROBE_SHORT_TIMEOUT, "pick", points["pick"]["coords"])
        move_strict(mc, points["pick_hover"]["angles"], PROBE_SHORT_SPEED, PROBE_SHORT_TIMEOUT, "pick_hover-return", points["pick_hover"]["coords"])
        move_strict(mc, HOME_ANGLES, PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "HOME-return")
    elif mode == "probe-drop":
        move_strict(mc, points["drop_hover"]["angles"], PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "drop_hover", points["drop_hover"]["coords"])
        move_strict(mc, points["drop"]["angles"], PROBE_SHORT_SPEED, PROBE_SHORT_TIMEOUT, "drop", points["drop"]["coords"])
        move_strict(mc, points["drop_hover"]["angles"], PROBE_SHORT_SPEED, PROBE_SHORT_TIMEOUT, "drop_hover-return", points["drop_hover"]["coords"])
        move_strict(mc, HOME_ANGLES, PROBE_LONG_SPEED, PROBE_LONG_TIMEOUT, "HOME-return")
    else:
        raise ValueError(f"未知 probe 模式: {mode}")
    print("[PASS] 受控诊断完成：严格到位，未发送夹爪命令。")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="myCobot 280 180°五点示教与受控诊断 v1")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--teach-only", action="store_true", help="五点示教并保存候选 JSON，不回零、不自动抓放")
    modes.add_argument("--probe-pick-hover", action="store_true", help="HOME->pick_hover->HOME，无夹爪")
    modes.add_argument("--probe-drop-hover", action="store_true", help="HOME->drop_hover->HOME，无夹爪")
    modes.add_argument("--probe-pick", action="store_true", help="HOME->pick_hover->pick->pick_hover->HOME，无夹爪")
    modes.add_argument("--probe-drop", action="store_true", help="HOME->drop_hover->drop->drop_hover->HOME，无夹爪")
    modes.add_argument("--check-preset", metavar="NAME_OR_FILE", help="离线校验点位和外部证据，不连接机械臂")
    modes.add_argument("--annotate-preset", metavar="NAME_OR_FILE", help="离线附加外部证据并输出新版本 JSON")
    parser.add_argument("--port", help="现场确认的 COM 口；不自动扫描")
    parser.add_argument("--preset", help="probe 使用的 JSON 裸名或文件路径")
    parser.add_argument("--save-preset", help="teach-only 保存的 JSON 裸名或路径")
    parser.add_argument("--save-preset-log", default="", help="来源日志编号")
    parser.add_argument("--save-preset-obj", default="", help="物块尺寸(cm)")
    parser.add_argument("--save-preset-notes", default="", help="示教备注")
    parser.add_argument("--external-reference-json", help="外部基准证据 JSON")
    parser.add_argument("--output-preset", help="--annotate-preset 的新输出 JSON")
    parser.add_argument("--log-file", help="审计日志路径；默认自动写入 audit_logs")
    parser.add_argument("--confirm-action-gate", action="store_true",
                        help="确认已完成 Codex Gate/T0-C/急停/净空并获现场动作许可")
    return parser


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = build_parser()
    args = parser.parse_args(argv)
    mode_flags = [args.teach_only, args.probe_pick_hover, args.probe_drop_hover,
                  args.probe_pick, args.probe_drop, bool(args.check_preset), bool(args.annotate_preset)]
    if sum(bool(v) for v in mode_flags) != 1:
        parser.error("必须显式指定且只能指定一种模式")
    if args.check_preset or args.annotate_preset:
        if args.port or args.preset or args.save_preset or args.confirm_action_gate:
            parser.error("离线点位模式不能与串口/动作参数组合")
        if args.check_preset and (args.external_reference_json or args.output_preset):
            parser.error("--check-preset 不能附带外部证据写入参数")
        if args.annotate_preset and (not args.external_reference_json or not args.output_preset):
            parser.error("--annotate-preset 必须同时提供 --external-reference-json 和 --output-preset")
        return args
    if not args.port:
        parser.error("真机示教/探针必须显式提供已确认的 --port")
    if not args.confirm_action_gate:
        parser.error("真机模式必须显式提供 --confirm-action-gate；未授权时不能连接/释放舵机")
    if args.teach_only:
        if args.preset or not args.save_preset:
            parser.error("--teach-only 必须保存新预设且不能同时加载旧预设")
    else:
        if not args.preset or args.save_preset:
            parser.error("probe 模式必须加载 --preset，不能同时 --save-preset")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.check_preset:
        try:
            load_and_validate(args.check_preset, require_external=True)
            return 0
        except Exception as exc:
            print(f"[FAIL] 离线点位校验: {type(exc).__name__}: {exc}")
            return 1
    if args.annotate_preset:
        try:
            annotate_preset(args.annotate_preset, args.external_reference_json, args.output_preset)
            return 0
        except Exception as exc:
            print(f"[FAIL] 外部证据附加: {type(exc).__name__}: {exc}")
            return 1
    log_path = Path(args.log_file) if args.log_file else AUDIT_DIR / f"180deg_teach_{time.strftime('%Y%m%d_%H%M%S')}.log"
    with capture_log(log_path):
        mc = None
        try:
            from pymycobot.mycobot import MyCobot
            print(f"[INFO] connecting to {args.port} @ {BAUD}")
            mc = MyCobot(args.port, BAUD)
            time.sleep(0.5)
            if args.teach_only:
                teach_only(mc, args)
            else:
                mode = next(name for name, flag in (
                    ("probe-pick-hover", args.probe_pick_hover),
                    ("probe-drop-hover", args.probe_drop_hover),
                    ("probe-pick", args.probe_pick),
                    ("probe-drop", args.probe_drop),
                ) if flag)
                # drop 侧直接决定最终 180°落点；没有外部基准证据时只允许做
                # 离线审查，不允许把候选点带入真机 drop 路径。
                require_external = mode in {"probe-drop-hover", "probe-drop"}
                _, data = load_and_validate(args.preset, require_external=require_external)
                probe(mc, data, mode)
            return 0
        except KeyboardInterrupt:
            print("[STOP] Ctrl+C；仅发送软件 stop，不自动继续动作。")
            if mc is not None:
                try:
                    mc.stop()
                except Exception as exc:
                    print(f"[WARN] stop 失败: {exc}")
            return 130
        except TeachAbort as exc:
            print(f"[ABORT] {exc}")
            return 2
        except Exception as exc:
            print(f"[FAIL] {type(exc).__name__}: {exc}")
            if mc is not None:
                try:
                    mc.stop()
                except Exception as stop_exc:
                    print(f"[WARN] stop 失败: {stop_exc}")
            return 1


if __name__ == "__main__":
    raise SystemExit(main())
