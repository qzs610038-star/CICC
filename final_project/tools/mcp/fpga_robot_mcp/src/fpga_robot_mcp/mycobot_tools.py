"""
fpga_robot_mcp.mycobot_tools — myCobot 280 环境/端口/只读状态工具。

实现函数:
- check_env() — 检查 pyserial/pymycobot 导入状态
- list_ports() — 列出串口并标注 CP210x 候选
- validate_connection() — 验证 myCobot 连接（只读握手）
- read_angles() — 读取关节角度（只读）
- read_coords() — 读取坐标（只读）
- set_rgb() — 设置 RGB 灯板（低风险非运动）
- plan_motion() — 规划动作（只读）
- execute_motion() — 执行动作（需 safety 门控）
- control_gripper() — 控制夹爪（需 safety 门控）
- stop_motion() — 停止/释放
- export_session_log() — 导出会话日志

依赖: pymycobot (可选), pyserial, serial_probe
"""

from __future__ import annotations

import datetime
import importlib.util
from typing import Any, Optional

from fpga_robot_mcp import serial_probe
from fpga_robot_mcp.config import load_config


def check_env() -> dict:
    """
    检查 myCobot 280 运行环境。

    返回:
    {
        "pyserial": "installed" | "missing",
        "pymycobot": "installed" | "missing",
        "python_version": "3.10.20",
        "serial_probe_available": true | false,
        "overall": "ready" | "partial" | "missing",
    }
    """
    py_version = f"{__import__('sys').version_info.major}.{__import__('sys').version_info.minor}.{__import__('sys').version_info.micro}"

    pyserial_spec = importlib.util.find_spec("serial")
    pyserial_status = "installed" if pyserial_spec else "missing"

    pymycobot_spec = importlib.util.find_spec("pymycobot")
    pymycobot_status = "installed" if pymycobot_spec else "missing"

    serial_probe_ok = True
    try:
        serial_probe.list_ports()
    except NotImplementedError:
        serial_probe_ok = False
    except Exception:
        serial_probe_ok = True  # pyserial 缺失不会导致 NotImplementedError

    # overall 判定
    if pyserial_status == "installed" and pymycobot_status == "installed":
        overall = "ready"
    elif pyserial_status == "missing" and pymycobot_status == "missing":
        overall = "missing"
    else:
        overall = "partial"

    return {
        "status": "ok",
        "data": {
            "pyserial": pyserial_status,
            "pymycobot": pymycobot_status,
            "python_version": py_version,
            "serial_probe_available": serial_probe_ok,
            "overall": overall,
        },
    }


def list_ports() -> dict:
    """
    列出串口并标注 myCobot 候选。

    委托 serial_probe.list_ports() 完成。
    """
    try:
        ports = serial_probe.list_ports()
    except NotImplementedError:
        return {"status": "not_implemented", "message": "serial_probe 未实现，无法列出端口"}

    candidates = [p for p in ports if p.get("is_mycobot_candidate")]
    return {
        "status": "ok",
        "data": {
            "all_ports": ports,
            "mycobot_candidates": candidates,
            "candidate_count": len(candidates),
            "total": len(ports),
        },
    }


def _create_mycobot(port: str, baudrate: int):
    """创建 MyCobot 实例，仅在 pymycobot 可用时。"""
    pymycobot_spec = importlib.util.find_spec("pymycobot")
    if not pymycobot_spec:
        raise ImportError("pymycobot 未安装")

    from pymycobot import MyCobot  # type: ignore[import-untyped]

    return MyCobot(port, baudrate)


def validate_connection(port: str, baudrate: int = 1000000) -> dict:
    """
    验证 myCobot 连接（只读握手）。

    参数:
        port: COM 口名称，如 "COM3"
        baudrate: 波特率，默认 1000000

    返回:
    {
        "port": port,
        "baudrate": baudrate,
        "connected": true,
        "robot_version": "..." | None,
        "message": "..."
    }
    """
    if not port:
        return {
            "status": "error",
            "message": "端口号不能为空",
            "data": {"port": port, "baudrate": baudrate, "connected": False, "robot_version": None},
        }

    try:
        import pymycobot
    except ImportError:
        return {
            "status": "error",
            "message": "pymycobot 未安装。请在 pfmval_py310 环境中执行: pip install pymycobot",
            "data": {"port": port, "baudrate": baudrate, "connected": False, "robot_version": None},
        }

    try:
        mc = pymycobot.MyCobot(port, baudrate)
        # 只读握手：获取版本号
        try:
            version = mc.get_robot_version()
        except Exception:
            version = None

        # 尝试获取系统版本作为备选验证
        if version is None:
            try:
                version_info = mc.get_system_version()
                version = str(version_info) if version_info else None
            except Exception:
                pass

        connected = version is not None
        if not connected:
            return {
                "status": "ok",
                "data": {
                    "port": port,
                    "baudrate": baudrate,
                    "connected": False,
                    "robot_version": None,
                    "message": (
                        f"端口 {port} 已打开但未收到机械臂应答。请检查：\n"
                        f"- USB-TTL 接线是否正确（TXD→RX, RXD→TX, GND→GND）\n"
                        f"- 波特率是否为 {baudrate}\n"
                        f"- 机械臂电源是否开启\n"
                        f"- 机械臂是否处于正确姿态（通电前须手动摆正）"
                    ),
                },
            }

        return {
            "status": "ok",
            "data": {
                "port": port,
                "baudrate": baudrate,
                "connected": True,
                "robot_version": str(version),
                "message": f"已连接到 myCobot 280 (端口: {port}, 波特率: {baudrate})",
            },
        }
    except Exception as e:
        err_msg = str(e)
        tips = []
        if "could not open port" in err_msg.lower():
            tips.append("端口不存在或被占用，请检查 USB-TTL 连接")
        elif "timeout" in err_msg.lower():
            tips.append("连接超时，请检查波特率(1000000)和机械臂电源")
        else:
            tips = [
                "检查 USB-TTL 线是否连接",
                "检查 CP210x 驱动是否安装",
                "检查机械臂电源是否开启",
                f"确认波特率是否为 {baudrate}",
            ]
        return {
            "status": "error",
            "message": f"连接失败: {err_msg}",
            "data": {
                "port": port,
                "baudrate": baudrate,
                "connected": False,
                "robot_version": None,
                "tips": tips,
            },
        }


def read_angles(port: str, baudrate: int = 1000000) -> dict:
    """
    读取 myCobot 280 的 6 个关节角度（只读）。

    返回:
    {
        "status": "ok",
        "angles": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        "joint_names": ["J1", "J2", "J3", "J4", "J5", "J6"],
    }
    """
    try:
        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装", "angles": None}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}", "angles": None}

    try:
        angles = mc.get_angles()
        if angles is None:
            return {
                "status": "error",
                "message": "读取角度失败，请检查连接和供电",
                "angles": None,
                "tips": ["检查 USB-TTL 连接", "检查 CP210x 驱动", "确认机械臂电源"],
            }
        return {
            "status": "ok",
            "angles": list(angles),
            "joint_names": ["J1", "J2", "J3", "J4", "J5", "J6"],
        }
    except Exception as e:
        return {
            "status": "error",
            "message": f"读取角度失败: {e}",
            "angles": None,
        }


def read_coords(port: str, baudrate: int = 1000000) -> dict:
    """
    读取 myCobot 280 当前坐标（只读）。

    返回:
    {
        "status": "ok",
        "coords": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        "units": "mm / degrees",
    }
    """
    try:
        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装", "coords": None}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}", "coords": None}

    try:
        coords = mc.get_coords()
        if coords is None:
            return {"status": "error", "message": "读取坐标失败", "coords": None}
        return {
            "status": "ok",
            "coords": list(coords),
            "units": "mm / degrees",
        }
    except Exception as e:
        return {"status": "error", "message": f"读取坐标失败: {e}", "coords": None}


def set_rgb(port: str, baudrate: int, r: int, g: int, b: int) -> dict:
    """
    设置 myCobot 280 RGB 灯板颜色。

    r/g/b 范围: 0-255
    """
    r = max(0, min(255, int(r)))
    g = max(0, min(255, int(g)))
    b = max(0, min(255, int(b)))

    try:
        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装"}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}"}

    try:
        mc.set_color(r, g, b)
        return {
            "status": "ok",
            "data": {"rgb": {"r": r, "g": g, "b": b}, "message": "RGB 灯板已设置"},
        }
    except Exception as e:
        return {"status": "error", "message": f"设置 RGB 失败: {e}"}


def plan_motion(target: str = "", angles: Optional[list[float]] = None) -> dict:
    """
    根据目标动作生成动作摘要（只读，不执行）。

    参数:
        target: 目标描述
        angles: 6 关节角度列表或 None
    """
    if angles is None:
        angles = []

    lines = ["## 动作规划摘要", ""]
    if target:
        lines.append(f"**目标**: {target}")
        lines.append("")

    if angles and len(angles) == 6:
        lines.append(f"**目标角度**: J1={angles[0]:.1f} J2={angles[1]:.1f} J3={angles[2]:.1f} "
                      f"J4={angles[3]:.1f} J5={angles[4]:.1f} J6={angles[5]:.1f}")
        angle_range = max(angles) - min(angles) if angles else 0
        lines.append(f"**运动范围**: {angle_range:.1f}°")
        lines.append("")
    elif angles:
        lines.append(f"**目标角度**: {angles} (期望 6 个关节，实际 {len(angles)} 个)")
        lines.append("")

    lines.append("**风险提示**:")
    lines.append("- 通电前确保机械臂处于正确姿态")
    lines.append("- 初始速度建议不超过 30")
    lines.append("- 确保工作空间无障碍物")
    lines.append("- 随时准备按下急停按钮")
    lines.append("")
    lines.append("> 本规划只读，不执行任何动作。")

    return {
        "status": "ok",
        "data": {
            "markdown": "\n".join(lines),
            "target": target,
            "angles": angles,
            "joint_count": len(angles),
        },
    }


def execute_motion(port: str, baudrate: int, angles: list[float], speed: int) -> dict:
    """
    执行关节动作。需 safety 门控。

    参数:
        port: COM 口
        baudrate: 波特率
        angles: 6 关节角度列表
        speed: 速度 (1-100)
    """
    # 参数校验
    if len(angles) != 6:
        return {"status": "error", "message": f"需要 6 个关节角度，实际收到 {len(angles)} 个"}

    # 角度范围校验（myCobot 280 机械限位安全边界）
    for i, a in enumerate(angles):
        if not isinstance(a, (int, float)):
            return {"status": "error", "message": f"关节 J{i+1} 的角度值不是有效的数字: {a}"}
        if a < -180 or a > 180:
            return {"status": "error", "message": f"关节 J{i+1} 角度 {a}° 超出物理范围 [-180, 180]"}

    cfg = load_config()
    max_speed = cfg.mycobot280.max_speed
    if speed < 1:
        return {"status": "error", "message": f"速度 {speed} 无效，速度必须 >= 1"}
    if speed > max_speed:
        return {"status": "error", "message": f"速度 {speed} 超过上限 {max_speed}，请降低速度"}

    try:
        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装"}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}"}

    try:
        mc.send_angles(angles, speed)
        return {
            "status": "ok",
            "data": {
                "target_angles": angles,
                "speed": speed,
                "message": "动作指令已发送",
            },
        }
    except Exception as e:
        return {"status": "error", "message": f"动作执行失败: {e}"}


def control_gripper(port: str, baudrate: int, gripper_open: bool, speed: int) -> dict:
    """
    控制 myCobot 280 夹爪。需 safety 门控。

    参数:
        port: COM 口
        baudrate: 波特率
        gripper_open: True 张开, False 闭合
        speed: 速度
    """
    try:
        # 速度校验
        cfg = load_config()
        max_speed = cfg.mycobot280.max_speed
        if speed < 1:
            return {"status": "error", "message": f"夹爪速度 {speed} 无效，速度必须 >= 1"}
        if speed > max_speed:
            return {"status": "error", "message": f"夹爪速度 {speed} 超过上限 {max_speed}，请降低速度"}

        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装"}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}"}

    try:
        # set_gripper_state(0, speed) — 张开; set_gripper_state(1, speed) — 闭合
        state = 0 if gripper_open else 1
        mc.set_gripper_state(state, speed)
        action = "张开" if gripper_open else "闭合"
        return {
            "status": "ok",
            "data": {"action": "open" if gripper_open else "close", "speed": speed, "message": f"夹爪已{action}"},
        }
    except Exception as e:
        return {"status": "error", "message": f"夹爪控制失败: {e}"}


def stop_motion(port: str, baudrate: int) -> dict:
    """
    发送停止/释放命令。
    """
    try:
        mc = _create_mycobot(port, baudrate)
    except ImportError:
        return {"status": "error", "message": "pymycobot 未安装"}
    except Exception as e:
        return {"status": "error", "message": f"连接失败: {e}"}

    try:
        mc.stop()
        return {"status": "ok", "data": {"message": "停止命令已发送"}}
    except Exception as e:
        try:
            mc.release_all_servos()
            return {"status": "ok", "data": {"message": "已释放所有舵机"}}
        except Exception:
            return {"status": "error", "message": f"停止失败: {e}"}


def export_session_log() -> dict:
    """
    导出本次连接、动作摘要、返回值和错误记录。

    从 safety 模块收集审计日志并写入证据目录。
    """
    from fpga_robot_mcp.safety import SafetyManager

    cfg = load_config()
    evidence_dir = cfg.evidence_path()
    evidence_dir.mkdir(parents=True, exist_ok=True)

    now = datetime.datetime.now()
    timestamp = now.strftime("%Y%m%d_%H%M%S")
    log_path = evidence_dir / f"mycobot_session_{timestamp}.md"

    # 收集最近的审计日志
    try:
        # 从全局 server 状态获取（如果有）
        from fpga_robot_mcp import server
        sm = server._safety
        if sm is None:
            sm = SafetyManager(cfg)
        entries = sm.get_recent_audit(50)
    except Exception:
        entries = []

    lines = [
        f"# myCobot 280 会话日志",
        f"",
        f"**时间**: {now.strftime('%Y-%m-%d %H:%M:%S')}",
        f"**来源**: fpga_robot_mcp MCP 服务",
        f"",
        f"## 审计条目",
        f"",
    ]
    if entries:
        for e in entries:
            status_icon = "✅" if e.get("approved") else "❌"
            lines.append(f"- {status_icon} **{e.get('tool', '?')}** | {e.get('level', '?')} | {e.get('detail', '')}")
    else:
        lines.append("*(无审计记录)*")

    summary_entry = {"status": "ok"}
    if entries:
        ok_count = sum(1 for e in entries if e.get("approved"))
        fail_count = sum(1 for e in entries if not e.get("approved"))
        lines.append("")
        lines.append("## 汇总")
        lines.append(f"- 总操作: {len(entries)}")
        lines.append(f"- 成功: {ok_count}")
        lines.append(f"- 失败: {fail_count}")
        summary_entry["entries_count"] = len(entries)
        summary_entry["summary"] = f"{len(entries)} 次操作，{ok_count} 次成功，{fail_count} 次失败"

    markdown = "\n".join(lines)
    try:
        log_path.write_text(markdown, encoding="utf-8")
        return {
            "status": "ok",
            "data": {
                "log_path": str(log_path),
                **summary_entry.get("data", {}),
                **({"entries_count": len(entries)} if entries else {"entries_count": 0}),
            },
        }
    except Exception as e:
        return {"status": "error", "message": f"写入日志失败: {e}"}
