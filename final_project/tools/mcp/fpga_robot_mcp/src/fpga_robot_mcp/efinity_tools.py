"""
fpga_robot_mcp.efinity_tools — Efinity 工具链校验工具。

本模块由以下两部分组成：
- ✅ 已实现：locate_toolchain()、check_install_prereq()、check_project()
  list_artifacts()、run_build()、check_programmer()、program_bitstream()、collect_logs()
- 工程/产物检查基于 os.path / pathlib。
- Programmer 探测和烧录通过 Efinity setup.bat + ftdi_program.py。

所有函数返回 dict，格式为 {"status": "ok", "data": ...} 或 {"status": "error", "message": ...}
"""

from __future__ import annotations

import datetime
import json
import os
import re
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

from fpga_robot_mcp.config import FpgaRobotConfig, load_config

# ── 内部工具 ────────────────────────────────────────────────


def _get_cfg() -> FpgaRobotConfig | None:
    """获取当前配置（尝试从模块全局缓存，无则重新加载）。"""
    try:
        return load_config()
    except Exception:
        return None


def _exists(*parts: str) -> bool:
    """联合路径并检查存在性。"""
    return os.path.exists(os.path.join(*parts))


def _quote_cmd_arg(value: object) -> str:
    """Quote a Windows cmd.exe argument for the simple paths used here."""
    return f'"{str(value).replace(chr(34), chr(34) + chr(34))}"'


def _build_ftdi_program_command(cfg: FpgaRobotConfig, args: list[str]) -> dict:
    """Build a Windows command that runs ftdi_program.py after Efinity setup.bat."""
    setup_bat = cfg.programmer_setup_path()
    ftdi_program = cfg.ftdi_program_path()
    python_command = cfg.programmer.python_command or "python"
    quoted_args = [_quote_cmd_arg(item) for item in [python_command, str(ftdi_program), *args]]
    python_invocation = " ".join(quoted_args)
    inner_command = f'call "{setup_bat}" >nul && {python_invocation}'
    return {
        "cmd": inner_command,
        "display_command": inner_command,
        "setup_bat": str(setup_bat),
        "setup_bat_exists": setup_bat.is_file(),
        "ftdi_program_py": str(ftdi_program),
        "ftdi_program_py_exists": ftdi_program.is_file(),
        "python_command": python_command,
    }


def _build_efx_run_command(
    cfg: FpgaRobotConfig,
    xml_path: Path,
    flow: str,
    output_dir: str = "",
    work_dir: str = "",
) -> dict:
    """Build an Efinity efx_run.bat project command."""
    setup_bat = cfg.programmer_setup_path()
    efx_run = cfg.efinity_bin_path() / "efx_run.bat"
    cwd = xml_path.parent
    args = [xml_path.name, "--prj", "-f", flow]
    if output_dir:
        args.extend(["--output_dir", output_dir])
    if work_dir:
        args.extend(["--work_dir", work_dir])

    invocation = " ".join([_quote_cmd_arg(str(efx_run)), *[_quote_cmd_arg(arg) for arg in args]])
    inner_command = f'call "{setup_bat}" >nul && {invocation}'
    return {
        "cmd": inner_command,
        "display_command": f'cd /d "{cwd}" && {inner_command}',
        "cwd": str(cwd),
        "setup_bat": str(setup_bat),
        "setup_bat_exists": setup_bat.is_file(),
        "efx_run": str(efx_run),
        "efx_run_exists": efx_run.is_file(),
        "args": args,
    }


_EFX_XML_NS = "http://www.efinixinc.com/enf_proj"
_XSI_XML_NS = "http://www.w3.org/2001/XMLSchema-instance"


def _set_efx_param(parent: ET.Element, name: str, value: str, value_type: str) -> None:
    for param in parent.findall(f"{{{_EFX_XML_NS}}}param"):
        if param.get("name") == name:
            param.set("value", value)
            param.set("value_type", value_type)
            return
    ET.SubElement(
        parent,
        f"{{{_EFX_XML_NS}}}param",
        {"name": name, "value": value, "value_type": value_type},
    )


def _prepare_build_project_xml(xml_path: Path, disable_debug: bool) -> tuple[Path, Path | None]:
    """Optionally create a temporary project XML with debugger auto insertion disabled."""
    if not disable_debug:
        return xml_path, None

    ET.register_namespace("efx", _EFX_XML_NS)
    ET.register_namespace("xsi", _XSI_XML_NS)
    tree = ET.parse(xml_path)
    root = tree.getroot()
    debugger = root.find(f"{{{_EFX_XML_NS}}}debugger")
    if debugger is None:
        debugger = ET.SubElement(root, f"{{{_EFX_XML_NS}}}debugger")
    _set_efx_param(debugger, "work_dir", "work_dbg", "e_string")
    _set_efx_param(debugger, "auto_instantiation", "off", "e_bool")
    _set_efx_param(debugger, "profile", "NONE", "e_string")

    synthesis = root.find(f"{{{_EFX_XML_NS}}}synthesis")
    if synthesis is not None:
        _set_efx_param(synthesis, "enable-mark-debug", "0", "e_option")

    generated = xml_path.with_name(f"{xml_path.stem}.codex_nodebug.xml")
    tree.write(generated, encoding="utf-8", xml_declaration=True)
    return generated, generated


def _cleanup_generated_project_xml(generated_project_xml: Path | None) -> dict:
    """Remove a temporary project XML generated for build-only options."""
    if generated_project_xml is None:
        return {"removed": False, "path": "", "error": ""}
    existed = generated_project_xml.exists()
    try:
        generated_project_xml.unlink(missing_ok=True)
        return {
            "removed": existed,
            "path": str(generated_project_xml),
            "error": "",
        }
    except Exception as e:
        return {
            "removed": False,
            "path": str(generated_project_xml),
            "error": str(e),
        }


def _run_ftdi_program(cfg: FpgaRobotConfig, args: list[str], timeout: int = 30) -> dict:
    """Run ftdi_program.py through setup.bat and return structured process output."""
    command = _build_ftdi_program_command(cfg, args)
    if not command["setup_bat_exists"]:
        return {
            "status": "missing_setup",
            "command": command,
            "return_code": None,
            "stdout": "",
            "stderr": f"setup.bat 不存在: {command['setup_bat']}",
        }
    if not command["ftdi_program_py_exists"]:
        return {
            "status": "missing_ftdi_program",
            "command": command,
            "return_code": None,
            "stdout": "",
            "stderr": f"ftdi_program.py 不存在: {command['ftdi_program_py']}",
        }

    try:
        result = subprocess.run(
            command["cmd"],
            shell=True,
            capture_output=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
        return {
            "status": "ok" if result.returncode == 0 else "process_failed",
            "command": command,
            "return_code": result.returncode,
            "stdout": result.stdout or "",
            "stderr": result.stderr or "",
        }
    except subprocess.TimeoutExpired as e:
        return {
            "status": "timeout",
            "command": command,
            "return_code": None,
            "stdout": e.stdout or "",
            "stderr": e.stderr or "ftdi_program.py 执行超时",
        }
    except Exception as e:
        return {
            "status": "error",
            "command": command,
            "return_code": None,
            "stdout": "",
            "stderr": str(e),
        }


_USB_TARGET_RE = re.compile(
    r"Usb Target:\s*(?P<name>.*?),\s*(?P<bus>Bus\s+\S+\s+Device\s+\S+):\s*ID\s+"
    r"(?P<vidpid>[0-9A-Fa-f:]+)(?:\s+S/N\s+(?P<serial>\S+))?"
)
_USB_URL_RE = re.compile(r"url\[(?P<index>\d+)\]:\s*(?P<url>.*)")
_JTAG_DEVICE_ID_RE = re.compile(r"Device ID read from JTAG:\s*(0x[0-9A-Fa-f]+)")
_INVALID_JTAG_DEVICE_IDS = {"0X00000000", "0XFFFFFFFF"}


def _parse_list_usb_output(stdout: str) -> list[dict]:
    """Parse ftdi_program.py --list_usb output into target dictionaries."""
    targets: list[dict] = []
    current: dict | None = None

    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        match = _USB_TARGET_RE.search(line)
        if match:
            current = {
                "name": match.group("name"),
                "bus": match.group("bus"),
                "vidpid": match.group("vidpid"),
                "serial_no": match.group("serial") or "",
                "urls": [],
            }
            targets.append(current)
            continue
        url_match = _USB_URL_RE.search(line)
        if current is not None and url_match:
            url = url_match.group("url").strip()
            if url and url.lower() != "none":
                current["urls"].append({
                    "index": int(url_match.group("index")),
                    "url": url,
                })

    return targets


def _parse_scan_usb_output(stdout: str) -> list[dict]:
    """Parse ftdi_program.py --scan_usb JSON output."""
    text = stdout.strip()
    if not text:
        return []
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        return [{"raw": text[:1000], "parse_error": "scan_usb 输出不是 JSON"}]
    if isinstance(parsed, dict):
        return [parsed]
    if isinstance(parsed, list):
        return [item for item in parsed if isinstance(item, dict)]
    return [{"raw": text[:1000], "parse_error": "scan_usb JSON 不是对象或列表"}]


def _pick_recommended_url(list_targets: list[dict]) -> str:
    for target in list_targets:
        for url_entry in target.get("urls", []):
            url = url_entry.get("url", "")
            if url:
                return url
    return ""


def _has_jtag_idcode(scan_targets: list[dict]) -> bool:
    for target in scan_targets:
        idcodes = target.get("idcode")
        if isinstance(idcodes, list) and len(idcodes) > 0:
            return True
        if isinstance(idcodes, str) and idcodes.strip():
            return True
    return False


def _extract_jtag_device_ids(stdout: str) -> list[str]:
    """Return normalized JTAG device IDs reported by ftdi_program.py."""
    return [match.group(1).upper() for match in _JTAG_DEVICE_ID_RE.finditer(stdout or "")]


def _valid_jtag_device_ids(device_ids: list[str]) -> list[str]:
    return [device_id for device_id in device_ids if device_id not in _INVALID_JTAG_DEVICE_IDS]


def _assess_program_result(mode: str, return_code: int | None, stdout: str, stderr: str) -> dict:
    """Classify programmer output beyond the process return code."""
    text = f"{stdout or ''}\n{stderr or ''}"
    device_ids = _extract_jtag_device_ids(stdout or "")
    valid_device_ids = _valid_jtag_device_ids(device_ids)
    finished = "finished with JTAG programming" in text

    if return_code != 0:
        return {
            "status": "program_failed",
            "message": f"烧录失败 (返回码: {return_code})",
            "device_ids": device_ids,
            "valid_device_ids": valid_device_ids,
            "finished": finished,
        }

    if mode in {"jtag", "jtag_chain"}:
        if not device_ids:
            return {
                "status": "program_suspicious",
                "message": "JTAG 烧录返回 0，但日志未包含 Device ID",
                "device_ids": device_ids,
                "valid_device_ids": valid_device_ids,
                "finished": finished,
            }
        if not valid_device_ids:
            return {
                "status": "program_suspicious",
                "message": f"JTAG Device ID 可疑: {', '.join(device_ids)}",
                "device_ids": device_ids,
                "valid_device_ids": valid_device_ids,
                "finished": finished,
            }
        if not finished:
            return {
                "status": "program_suspicious",
                "message": "JTAG 烧录返回 0 且 Device ID 有效，但缺少 finished 标志",
                "device_ids": device_ids,
                "valid_device_ids": valid_device_ids,
                "finished": finished,
            }

    return {
        "status": "ok",
        "message": "烧录成功",
        "device_ids": device_ids,
        "valid_device_ids": valid_device_ids,
        "finished": finished,
    }


def _has_scan_usb_target(scan_targets: list[dict]) -> bool:
    """Return true only for parsed scan_usb target objects, not raw error text."""
    for target in scan_targets:
        if target and not target.get("parse_error"):
            return True
    return False


def _artifact_outflow_dirs(cfg: FpgaRobotConfig) -> list[tuple[str, Path]]:
    """Return candidate outflow dirs for the formal project and external burn copy."""
    candidates = [
        ("configured_project", cfg.efinity_project_path().parent / "outflow"),
    ]
    burn_root = cfg.burn_project_path()
    if str(burn_root):
        candidates.append(("burn_project", burn_root / "fpga" / "efinity" / "outflow"))

    seen: set[str] = set()
    unique: list[tuple[str, Path]] = []
    for label, path in candidates:
        key = str(path.resolve()) if path.exists() else str(path.absolute())
        if key in seen:
            continue
        seen.add(key)
        unique.append((label, path))
    return unique


# ══════════════════════════════════════════════════════════════
# ✅ 已实现
# ══════════════════════════════════════════════════════════════


def locate_toolchain() -> dict:
    """
    搜索 Efinity 安装路径、CLI 可执行文件。

    检查顺序:
    1. D:\\Efinity\\2025.2\\bin\\efx_*.exe
    2. EFINITY_HOME 环境变量
    3. 注册表 EFINITY_HOME
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    efinity_home = cfg.efinity.home
    efinity_bin = cfg.efinity.bin

    checks = {}
    found_tools = []

    # 1. 检查安装目录
    checks["home_dir"] = os.path.isdir(efinity_home)
    checks["bin_dir"] = os.path.isdir(efinity_bin)

    required_tools = ["efx_map.exe", "efx_pgm.exe", "efx_pnr.exe", "efx_run.bat"]
    optional_tools = ["efx_simulate.exe"]

    # 2. 检查 CLI 可执行文件
    if checks["bin_dir"]:
        for tool in [*required_tools, *optional_tools]:
            tool_path = os.path.join(efinity_bin, tool)
            exists = os.path.isfile(tool_path)
            checks[tool] = exists
            if exists:
                size_kb = os.path.getsize(tool_path) // 1024
                found_tools.append({"name": tool, "path": tool_path, "size_kb": size_kb})

    # 3. 检查环境变量
    env_home = os.environ.get("EFINITY_HOME", "")
    checks["EFINITY_HOME_env"] = bool(env_home)
    checks["EFINITY_HOME_value"] = env_home if env_home else None

    # 4. 检查 device database
    arch_ini = os.path.join(efinity_home, "arch", "efxdevicedb.ini")
    checks["device_db"] = os.path.isfile(arch_ini)

    # 5. 检查 JTAG bitstream
    pgm_fli = cfg.efinity.pgm_fli
    checks["jtag_bitstream_dir"] = os.path.isdir(pgm_fli)

    # 6. 检查真实 Programmer CLI
    setup_bat = cfg.programmer_setup_path()
    ftdi_program = cfg.ftdi_program_path()
    checks["setup.bat"] = setup_bat.is_file()
    checks["ftdi_program.py"] = ftdi_program.is_file()

    required_ok = (
        checks["home_dir"]
        and checks["bin_dir"]
        and checks["device_db"]
        and checks["setup.bat"]
        and checks["ftdi_program.py"]
        and all(checks.get(tool, False) for tool in required_tools)
    )
    optional_missing = [tool for tool in optional_tools if not checks.get(tool, False)]

    return {
        "status": "ok" if required_ok else ("partial" if checks.get("home_dir") else "missing"),
        "data": {
            "efinity_home": efinity_home,
            "efinity_bin": efinity_bin,
            "checks": checks,
            "found_tools": found_tools,
            "required_tools": required_tools,
            "optional_tools": optional_tools,
            "optional_missing": optional_missing,
            "is_installed": required_ok,
        },
        "message": (
            f"Efinity 工具链{'已安装' if required_ok else '部分安装' if checks.get('home_dir') else '未安装'}"
        ),
    }


def check_install_prereq() -> dict:
    """
    检查 Phase -1 安装前置条件。

    包括:
    - RAR 解压工具（WinRAR / 7-Zip）
    - Efinity 安装包 RAR
    - 补丁脚本
    - 磁盘空间
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    root = cfg.project_root
    checks = {}

    # 1. 解压工具
    rar_tools = {
        "WinRAR": r"C:\Program Files\WinRAR\unrar.exe",
        "7z": r"C:\Program Files\7-Zip\7z.exe",
    }
    for name, path in rar_tools.items():
        checks[f"rar_tool_{name}"] = os.path.isfile(path) or bool(shutil.which(name.lower()))

    # 2. 安装包 RAR
    efinity_rar = os.path.join(root, "赛方提供材料", "EDA软件", "00 Efinity 2025.2.rar")
    checks["efinity_rar"] = os.path.isfile(efinity_rar)
    if checks["efinity_rar"]:
        checks["efinity_rar_size_gb"] = round(os.path.getsize(efinity_rar) / (1024**3), 2)

    # 3. 补丁脚本
    patch_dir = os.path.join(root, "赛方提供材料", "efinity-2025.2.288.4.15-windows-x64-patch")
    patch_bat = os.path.join(patch_dir, "run.bat")
    checks["patch_dir"] = os.path.isdir(patch_dir)
    checks["patch_run_bat"] = os.path.isfile(patch_bat)

    # 4. 安装目标路径的磁盘空间（D 盘）
    try:
        total, used, free = shutil.disk_usage("D:/")
        checks["disk_total_gb"] = round(total / (1024**3), 2)
        checks["disk_free_gb"] = round(free / (1024**3), 2)
        checks["disk_enough"] = free > 10 * (1024**3)  # 至少 10 GB 剩余
    except Exception:
        checks["disk_enough"] = None

    # 5. 安装状态
    checks["already_installed"] = os.path.isdir(cfg.efinity.home)

    return {
        "status": "ok",
        "data": checks,
    }


def check_project() -> dict:
    """
    校验主工程文件。

    检查:
    - mem_test.xml 存在且可读
    - constrain.sdc 存在且可读
    - 器件型号匹配
    - outflow 目录存在
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    checks = {}

    # 关键文件路径
    project_xml = cfg.efinity_project_path()
    constrain_sdc = cfg.constrain_sdc_path()

    checks["project_xml_exists"] = project_xml.is_file()
    checks["constrain_sdc_exists"] = constrain_sdc.is_file()

    # 读取 XML 确认内容（只读前几行）
    if checks["project_xml_exists"]:
        try:
            content = project_xml.read_text(encoding="utf-8", errors="ignore")
            checks["project_xml_size_kb"] = len(content) // 1024
            checks["contains_device"] = cfg.efinity.device in content
        except Exception as e:
            checks["project_xml_read_error"] = str(e)

    # SDC 基本检查
    if checks["constrain_sdc_exists"]:
        try:
            sdc_content = constrain_sdc.read_text(encoding="utf-8", errors="ignore")
            checks["constrain_sdc_size_kb"] = len(sdc_content) // 1024
        except Exception as e:
            checks["constrain_sdc_read_error"] = str(e)

    # outflow 目录（正式工程 + 外部烧录目录）
    outflow_summary = []
    for label, outflow in _artifact_outflow_dirs(cfg):
        entry = {
            "label": label,
            "path": str(outflow),
            "exists": outflow.is_dir(),
            "file_count": 0,
            "size_mb": 0.0,
        }
        if outflow.is_dir():
            try:
                files = list(outflow.rglob("*"))
                entry["file_count"] = len(files)
                entry["size_mb"] = round(
                    sum(f.stat().st_size for f in files if f.is_file()) / (1024**2), 2
                )
            except Exception as e:
                entry["error"] = str(e)
        outflow_summary.append(entry)
    checks["outflows"] = outflow_summary
    checks["outflow_exists"] = any(item["exists"] for item in outflow_summary)

    return {
        "status": "ok",
        "data": {
            "project_root": cfg.project_root,
            "device": cfg.efinity.device,
            "timing_model": cfg.efinity.timing_model,
            "checks": checks,
        },
    }


# ══════════════════════════════════════════════════════════════
# ⬅️ 留待经济模型填充
#
# 以下函数为桩代码，需要填充真实实现。
# 实现说明详见 Phase 1 实施策划第 4.2 节。
# ══════════════════════════════════════════════════════════════


def list_artifacts(limit: int = 30, offset: int = 0) -> dict:
    """
    列出 outflow 构建产物。

    遍历 outflow 目录中的 *.bit、*.rpt、*.log、*.txt 等文件，
    按修改时间倒序排列，应用 limit/offset 分页。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    # 收集产物文件
    artifacts = []
    artifact_roots = []
    try:
        for label, outflow in _artifact_outflow_dirs(cfg):
            root_entry = {"label": label, "outflow_path": str(outflow), "exists": outflow.is_dir()}
            artifact_roots.append(root_entry)
            if not outflow.is_dir():
                continue
            for f in outflow.rglob("*"):
                if not f.is_file():
                    continue
                ext = f.suffix.lower()
                if ext not in (
                    ".bit", ".hex", ".lbf", ".rpt", ".log", ".txt", ".bin",
                    ".svf", ".csv", ".json", ".xml", ".htm", ".html",
                ):
                    continue
                if ext == ".bit":
                    ftype = "bit"
                elif ext == ".hex":
                    ftype = "hex"
                elif ext == ".lbf":
                    ftype = "lbf"
                elif ext == ".rpt":
                    ftype = "rpt"
                elif ext in (".log", ".txt"):
                    ftype = "log"
                else:
                    ftype = "other"

                stat = f.stat()
                try:
                    mtime = datetime.datetime.fromtimestamp(stat.st_mtime).strftime("%Y-%m-%dT%H:%M:%S")
                except Exception:
                    mtime = ""

                rel_path = f.relative_to(outflow) if outflow in f.parents else f.name

                artifacts.append({
                    "source": label,
                    "path": str(rel_path),
                    "absolute_path": str(f),
                    "size_bytes": stat.st_size,
                    "last_modified": mtime,
                    "mtime_epoch": stat.st_mtime,
                    "type": ftype,
                })
    except Exception as e:
        return {"status": "error", "message": f"遍历 outflow 目录失败: {e}"}

    # 按修改时间倒序
    artifacts.sort(key=lambda x: x.get("mtime_epoch", 0), reverse=True)
    total = len(artifacts)

    # 分页
    paged = artifacts[offset:offset + limit] if offset < total else []
    for item in paged:
        item.pop("mtime_epoch", None)

    return {
        "status": "ok",
        "data": {
            "total": total,
            "artifacts": paged,
            "limit": limit,
            "offset": offset,
            "artifact_roots": artifact_roots,
        },
    }


def run_build(
    project_xml: str = "",
    dry_run: bool = True,
    flow: str = "compile",
    output_dir: str = "",
    work_dir: str = "",
    timeout_seconds: int = 1800,
    disable_debug: bool = False,
) -> dict:
    """
    运行 Efinity 综合/布局布线。

    dry_run=True: 只做预检查，不启动子进程。
    dry_run=False: 调用 efx_run.bat 执行构建。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    # 确定工程 XML 路径
    if project_xml:
        xml_path = Path(project_xml)
        if not xml_path.is_absolute():
            xml_path = cfg.resolve(project_xml)
    else:
        xml_path = cfg.efinity_project_path()

    supported_flows = {
        "map", "interface", "pnr", "pgm", "compile", "program", "rtlsim",
        "mapsim", "pnrsim", "full", "ptsimrtl", "ptsimfc", "sta_tclsh",
        "setup_efxlib",
    }
    flow = (flow or "compile").lower()
    if flow not in supported_flows:
        return {
            "status": "error",
            "message": f"不支持的 Efinity flow: {flow}",
            "data": {"supported_flows": sorted(supported_flows)},
        }

    effective_xml_path = xml_path
    generated_project_xml: Path | None = None
    if disable_debug and not dry_run:
        effective_xml_path, generated_project_xml = _prepare_build_project_xml(xml_path, disable_debug=True)

    command = _build_efx_run_command(cfg, effective_xml_path, flow, output_dir, work_dir)
    checks = {
        "project_xml": str(xml_path),
        "effective_project_xml": str(effective_xml_path),
        "generated_project_xml": str(generated_project_xml) if generated_project_xml else "",
        "disable_debug": disable_debug,
        "project_xml_exists": xml_path.is_file(),
        "setup_bat": command["setup_bat"],
        "setup_bat_exists": command["setup_bat_exists"],
        "efx_run": command["efx_run"],
        "efx_run_exists": command["efx_run_exists"],
        "flow": flow,
        "cwd": command["cwd"],
        "command": command["display_command"],
        "expected_outflow": str(xml_path.parent / "outflow"),
    }

    if not checks["project_xml_exists"]:
        return {"status": "error", "message": f"工程 XML 不存在: {xml_path}", "data": checks}

    if not checks["setup_bat_exists"]:
        return {"status": "error", "message": f"setup.bat 不存在: {command['setup_bat']}", "data": checks}

    if not checks["efx_run_exists"]:
        return {"status": "error", "message": f"efx_run.bat 不存在: {command['efx_run']}", "data": checks}

    if dry_run:
        if disable_debug:
            effective_preview = xml_path.with_name(f"{xml_path.stem}.codex_nodebug.xml")
            preview_command = _build_efx_run_command(cfg, effective_preview, flow, output_dir, work_dir)
            checks["effective_project_xml"] = str(effective_preview)
            checks["generated_project_xml"] = str(effective_preview)
            checks["command"] = preview_command["display_command"]
            checks["cwd"] = preview_command["cwd"]
        return {
            "status": "ok",
            "data": {
                "dry_run": True,
                **checks,
                "message": "dry-run 预检查通过，可执行构建",
                "next_step": f"设置 dry_run=false 并确认后执行: {checks['command']}",
            },
        }

    # 实际执行构建
    try:
        build_env = os.environ.copy()
        # Efinity ships its own Python. Host-side PYTHONPATH/PYTHONHOME can break
        # its startup, especially when the MCP package is imported via PYTHONPATH.
        build_env.pop("PYTHONPATH", None)
        build_env.pop("PYTHONHOME", None)

        result = subprocess.run(
            command["cmd"],
            capture_output=True,
            timeout=timeout_seconds,
            encoding="utf-8",
            errors="replace",
            shell=True,
            cwd=command["cwd"],
            env=build_env,
        )

        log_path = cfg.evidence_path() / f"build_{flow}_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_content = (
            f"=== Efinity Build Log ===\n"
            f"Command: {command['display_command']}\n"
            f"Return code: {result.returncode}\n"
            f"Time: {datetime.datetime.now().isoformat()}\n\n"
            f"--- STDOUT ---\n{result.stdout}\n\n"
            f"--- STDERR ---\n{result.stderr}\n"
        )
        log_path.write_text(log_content, encoding="utf-8")
        generated_xml_cleanup = _cleanup_generated_project_xml(generated_project_xml)

        return {
            "status": "ok" if result.returncode == 0 else "build_failed",
            "data": {
                "dry_run": False,
                **checks,
                "generated_project_xml_cleanup": generated_xml_cleanup,
                "return_code": result.returncode,
                "log_path": str(log_path),
                "stdout": result.stdout[:1000] if result.stdout else "",
                "stderr": result.stderr[:500] if result.stderr else "",
                "message": f"构建{'成功' if result.returncode == 0 else '失败 (返回码: ' + str(result.returncode) + ')'}",
            },
        }
    except subprocess.TimeoutExpired:
        checks["generated_project_xml_cleanup"] = _cleanup_generated_project_xml(generated_project_xml)
        return {"status": "error", "message": f"构建超时（{timeout_seconds} 秒）", "data": checks}
    except Exception as e:
        checks["generated_project_xml_cleanup"] = _cleanup_generated_project_xml(generated_project_xml)
        return {"status": "error", "message": f"构建异常: {e}", "data": checks}


def check_programmer() -> dict:
    """
    检查下载线/烧录器。

    定位 ftdi_program.py，通过 --list_usb / --scan_usb 做只读探测。
    本工具只读，不执行烧录。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    command_info = _build_ftdi_program_command(cfg, [])
    checks = {
        "efx_pgm_path": str(cfg.efinity_bin_path() / "efx_pgm.exe"),
        "efx_pgm_exists": (cfg.efinity_bin_path() / "efx_pgm.exe").is_file(),
        "setup_bat": command_info["setup_bat"],
        "setup_bat_exists": command_info["setup_bat_exists"],
        "ftdi_program_py": command_info["ftdi_program_py"],
        "ftdi_program_py_exists": command_info["ftdi_program_py_exists"],
    }

    if not checks["setup_bat_exists"] or not checks["ftdi_program_py_exists"]:
        return {
            "status": "programmer_cli_missing",
            "data": {
                **checks,
                "message": "Efinity Programmer CLI 未对齐：setup.bat 或 ftdi_program.py 不存在",
            },
        }

    # 检查 FTDI 驱动（Windows 下检查设备管理器信息）
    driver_status = "unknown"
    try:
        # 尝试通过 pnputil 检查
        driver_result = subprocess.run(
            ["pnputil", "/enum-devices", "/deviceclass", "Ports"],
            capture_output=True, timeout=10, encoding="utf-8", errors="replace",
        )
        if "FT4232" in driver_result.stdout or "4232" in driver_result.stdout:
            driver_status = "ft4232_found"
        elif "FT232" in driver_result.stdout:
            driver_status = "ft232_found"
        elif "oem105" in driver_result.stdout.lower() or "libusb" in driver_result.stdout.lower():
            driver_status = "libusb_installed"
        else:
            driver_status = "no_ftdi_detected"
    except Exception:
        driver_status = "check_failed"

    list_usb = _run_ftdi_program(cfg, ["--list_usb"], timeout=30)
    scan_usb = _run_ftdi_program(cfg, ["--scan_usb"], timeout=30)
    list_targets = _parse_list_usb_output(list_usb.get("stdout", ""))
    scan_targets = _parse_scan_usb_output(scan_usb.get("stdout", ""))
    recommended_url = cfg.programmer.default_url or _pick_recommended_url(list_targets)
    usb_visible = bool(list_targets or _has_scan_usb_target(scan_targets))
    jtag_idcode_visible = _has_jtag_idcode(scan_targets)

    if jtag_idcode_visible:
        status = "ok"
        message = "Programmer USB 与 JTAG IDCODE 均可见"
    elif usb_visible:
        status = "usb_visible_no_idcode"
        message = "Programmer USB 可见，但 scan_usb 未读到 JTAG IDCODE；暂不满足自动烧录前置"
    else:
        status = "no_hardware"
        message = "未发现可用 FTDI Programmer USB target"

    return {
        "status": status,
        "data": {
            **checks,
            "driver_status": driver_status,
            "list_usb": {
                "status": list_usb.get("status"),
                "return_code": list_usb.get("return_code"),
                "targets": list_targets,
                "stdout": list_usb.get("stdout", "")[:1000],
                "stderr": list_usb.get("stderr", "")[:1000],
                "command": list_usb.get("command", {}).get("display_command"),
            },
            "scan_usb": {
                "status": scan_usb.get("status"),
                "return_code": scan_usb.get("return_code"),
                "targets": scan_targets,
                "stdout": scan_usb.get("stdout", "")[:1000],
                "stderr": scan_usb.get("stderr", "")[:1000],
                "command": scan_usb.get("command", {}).get("display_command"),
            },
            "usb_visible": usb_visible,
            "jtag_idcode_visible": jtag_idcode_visible,
            "recommended_url": recommended_url,
            "ready_for_jtag_program": (
                jtag_idcode_visible if cfg.programmer.require_jtag_idcode else usb_visible
            ),
            "message": message,
        },
    }


def program_bitstream(
    bitstream_path: str,
    mode: str = "",
    url: str = "",
    dry_run: bool = True,
) -> dict:
    """
    烧录 bitstream 到 FPGA。

    警告: 硬件副作用操作，需通过 safety 门控 + confirm_token。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    bs = Path(bitstream_path)
    if not bs.is_absolute():
        bs = cfg.resolve(bitstream_path)
    if not bs.is_file():
        return {"status": "error", "message": f"bitstream 文件不存在: {bitstream_path}"}

    mode = mode or cfg.programmer.default_mode or "jtag"
    ext = bs.suffix.lower()
    allowed_by_mode = {
        "jtag": {".bit"},
        "jtag_chain": {".bit"},
        "passive": {".hex"},
        "active": {".hex"},
    }
    if mode not in allowed_by_mode:
        return {
            "status": "error",
            "message": f"不支持的烧录模式: {mode}",
            "data": {"supported_modes": sorted(allowed_by_mode)},
        }
    if ext not in allowed_by_mode[mode]:
        return {
            "status": "error",
            "message": f"{mode} 模式要求 {sorted(allowed_by_mode[mode])} 文件，当前是 {ext}",
            "data": {"bitstream": str(bs), "mode": mode},
        }

    programmer = check_programmer()
    programmer_data = programmer.get("data", {})
    recommended_url = url or programmer_data.get("recommended_url") or cfg.programmer.default_url
    args = [str(bs), "-m", mode]
    if recommended_url:
        args.extend(["--url", recommended_url])
    command = _build_ftdi_program_command(cfg, args)

    if dry_run:
        return {
            "status": "dry_run",
            "data": {
                "dry_run": True,
                "bitstream": str(bs),
                "mode": mode,
                "url": recommended_url,
                "command": command["display_command"],
                "programmer_status": programmer.get("status"),
                "programmer_ready": programmer_data.get("ready_for_jtag_program", False),
                "programmer_message": programmer_data.get("message", ""),
                "next_step": (
                    "确认 Programmer ready、目标 bitstream 和当前对话授权后，"
                    "设置 dry_run=false 并提供 EFINITY_PROGRAM confirm_token。"
                ),
            },
        }

    if not programmer_data.get("ready_for_jtag_program", False):
        return {
            "status": "programmer_not_ready",
            "message": "Programmer 未满足自动烧录前置，拒绝执行真实烧录",
            "data": {
                "bitstream": str(bs),
                "mode": mode,
                "url": recommended_url,
                "programmer_status": programmer.get("status"),
                "programmer_message": programmer_data.get("message", ""),
            },
        }

    start_time = time.time()

    try:
        result = _run_ftdi_program(cfg, args, timeout=180)
        duration = round(time.time() - start_time, 1)

        log_path = cfg.evidence_path() / f"program_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(
            f"=== Efinity Program Log ===\n"
            f"Bitstream: {bs}\n"
            f"Mode: {mode}\n"
            f"URL: {recommended_url}\n"
            f"Command: {command['display_command']}\n"
            f"Return code: {result.get('return_code')}\n"
            f"Duration: {duration}s\n\n"
            f"--- STDOUT ---\n{result.get('stdout', '')}\n\n"
            f"--- STDERR ---\n{result.get('stderr', '')}\n",
            encoding="utf-8",
        )

        assessment = _assess_program_result(
            mode,
            result.get("return_code"),
            result.get("stdout", ""),
            result.get("stderr", ""),
        )
        success = assessment["status"] == "ok"
        return {
            "status": assessment["status"],
            "data": {
                "bitstream": str(bs),
                "mode": mode,
                "url": recommended_url,
                "command": command["display_command"],
                "return_code": result.get("return_code"),
                "duration_seconds": duration,
                "log_path": str(log_path),
                "device_ids": assessment["device_ids"],
                "valid_device_ids": assessment["valid_device_ids"],
                "jtag_finished": assessment["finished"],
                "message": f"{assessment['message']} ({duration}s)",
            },
        }
    except Exception as e:
        return {"status": "error", "message": f"烧录异常: {e}"}


def collect_logs(log_dir: str = "") -> dict:
    """
    收集构建/烧录日志到证据目录。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    evidence = cfg.evidence_path()
    evidence.mkdir(parents=True, exist_ok=True)

    collected = []

    # 如果指定了 log_dir，从该目录收集
    if log_dir:
        src = Path(log_dir)
        if src.is_dir():
            for f in src.glob("*"):
                if f.is_file() and f.suffix.lower() in (".log", ".rpt", ".txt"):
                    dst = evidence / f.name
                    try:
                        import shutil
                        shutil.copy2(str(f), str(dst))
                        collected.append({"src": str(f), "dst": str(dst), "size_kb": f.stat().st_size // 1024})
                    except Exception as e:
                        collected.append({"src": str(f), "error": str(e)})
        else:
            return {"status": "error", "message": f"日志目录不存在: {log_dir}"}

    # 也自动尝试搜索正式工程和 burn copy 的 outflow 日志
    for label, outflow in _artifact_outflow_dirs(cfg):
        if not outflow.is_dir():
            continue
        for f in sorted(outflow.rglob("*"), key=lambda p: p.stat().st_mtime if p.is_file() else 0, reverse=True):
            if not f.is_file():
                continue
            if f.suffix.lower() not in (".rpt", ".log", ".txt"):
                continue
            dst = evidence / f"{label}_{f.name}"
            if dst.exists():
                continue
            try:
                import shutil
                shutil.copy2(str(f), str(dst))
                collected.append({
                    "source": label,
                    "src": str(f),
                    "dst": str(dst),
                    "size_kb": f.stat().st_size // 1024,
                })
            except Exception:
                pass

    return {
        "status": "ok",
        "data": {
            "evidence_dir": str(evidence),
            "collected_count": len(collected),
            "collected": collected,
        },
    }
