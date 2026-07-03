"""
fpga_robot_mcp.efinity_tools — Efinity 工具链校验工具。

本模块由以下两部分组成：
- ✅ 已实现：locate_toolchain()、check_install_prereq()、check_project()
  基于 os.path / pathlib 文件存在性检查，不涉及子进程调用。
- ⬅️ 待实现（留待经济模型填充）：
  list_artifacts()、run_build()、check_programmer()、program_bitstream()、collect_logs()

所有函数返回 dict，格式为 {"status": "ok", "data": ...} 或 {"status": "error", "message": ...}
"""

from __future__ import annotations

import datetime
import locale
import os
import shutil
import subprocess
import time
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


def _subprocess_text_encoding() -> str:
    """Windows 工具常按本机代码页输出，避免强制 UTF-8 造成中文乱码。"""
    return locale.getpreferredencoding(False) or "utf-8"


# ══════════════════════════════════════════════════════════════
# ✅ 已实现
# ══════════════════════════════════════════════════════════════


def locate_toolchain() -> dict:
    """
    搜索 Efinity 安装路径、CLI 可执行文件。

    检查顺序:
    1. D:\Efinity\2025.2\bin\efx_*.exe
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

    # 2. 检查 CLI 可执行文件。efx_simulate.exe 在部分 Windows 安装中不存在，
    #    不应影响核心综合/布局布线/烧录能力判断。
    core_tools = ["efx_map.exe", "efx_pgm.exe", "efx_pnr.exe", "efx_run.bat"]
    optional_tools = ["efx_simulate.exe"]
    if checks["bin_dir"]:
        for tool in core_tools + optional_tools:
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

    all_found = (
        checks.get("home_dir", False)
        and checks.get("bin_dir", False)
        and all(checks.get(tool, False) for tool in core_tools)
    )

    return {
        "status": "ok" if all_found else ("partial" if checks.get("home_dir") else "missing"),
        "data": {
            "efinity_home": efinity_home,
            "efinity_bin": efinity_bin,
            "checks": checks,
            "found_tools": found_tools,
            "core_tools": core_tools,
            "optional_tools": optional_tools,
            "is_installed": all_found,
        },
        "message": (
            f"Efinity 工具链{'已安装' if all_found else '部分安装' if checks.get('home_dir') else '未安装'}"
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

    # outflow 目录（产物）
    outflow = cfg.resolve("赛方提供材料", "TJ375N529_SC431HAI2LCD_Demo_V3", "outflow")
    checks["outflow_exists"] = outflow.is_dir()
    if checks["outflow_exists"]:
        try:
            files = list(outflow.rglob("*"))
            checks["outflow_file_count"] = len(files)
            checks["outflow_size_mb"] = round(
                sum(f.stat().st_size for f in files if f.is_file()) / (1024**2), 2
            )
        except Exception:
            pass

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

    outflow = cfg.resolve("赛方提供材料", "TJ375N529_SC431HAI2LCD_Demo_V3", "outflow")
    if not outflow.is_dir():
        return {
            "status": "ok",
            "data": {
                "total": 0,
                "artifacts": [],
                "outflow_path": str(outflow),
                "message": "outflow 目录不存在",
            },
        }

    # 收集产物文件
    artifacts = []
    try:
        for f in outflow.rglob("*"):
            if not f.is_file():
                continue
            ext = f.suffix.lower()
            if ext not in (".bit", ".rpt", ".log", ".txt", ".bin", ".svf", ".csv", ".json", ".xml", ".htm", ".html"):
                continue
            # 分类
            if ext == ".bit":
                ftype = "bit"
            elif ext == ".rpt":
                ftype = "rpt"
            elif ext in (".log", ".txt"):
                ftype = "log"
            else:
                ftype = "other"

            try:
                mtime = datetime.datetime.fromtimestamp(f.stat().st_mtime).strftime("%Y-%m-%dT%H:%M:%S")
            except Exception:
                mtime = ""

            rel_path = f.relative_to(outflow) if outflow in f.parents else f.name

            artifacts.append({
                "path": str(rel_path),
                "size_bytes": f.stat().st_size,
                "last_modified": mtime,
                "type": ftype,
            })
    except Exception as e:
        return {"status": "error", "message": f"遍历 outflow 目录失败: {e}"}

    # 按修改时间倒序
    artifacts.sort(key=lambda x: x.get("last_modified", ""), reverse=True)
    total = len(artifacts)

    # 分页
    paged = artifacts[offset:offset + limit] if offset < total else []

    return {
        "status": "ok",
        "data": {
            "total": total,
            "artifacts": paged,
            "limit": limit,
            "offset": offset,
            "outflow_path": str(outflow),
        },
    }


def run_build(project_xml: str = "", dry_run: bool = True) -> dict:
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

    # 定位 efx_run
    efx_run = cfg.efinity_bin_path() / "efx_run.bat"

    checks = {
        "project_xml": str(xml_path),
        "project_xml_exists": xml_path.is_file(),
        "efx_run": str(efx_run),
        "efx_run_exists": efx_run.is_file(),
    }

    if not checks["project_xml_exists"]:
        return {"status": "error", "message": f"工程 XML 不存在: {xml_path}", "data": checks}

    if not checks["efx_run_exists"]:
        return {"status": "error", "message": f"efx_run.bat 不存在: {efx_run}", "data": checks}

    if dry_run:
        return {
            "status": "ok",
            "data": {
                "dry_run": True,
                **checks,
                "message": "dry-run 预检查通过，可执行构建",
                "next_step": f"设置 dry_run=false 并确认后执行: {efx_run} -f {xml_path}",
            },
        }

    # 实际执行构建
    try:
        result = subprocess.run(
            [str(efx_run), "-f", str(xml_path)],
            capture_output=True,
            timeout=1800,  # 30 分钟超时
            encoding="utf-8",
            errors="replace",
            shell=True,
        )

        log_path = cfg.evidence_path() / f"build_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_content = (
            f"=== Efinity Build Log ===\n"
            f"Command: {efx_run} -f {xml_path}\n"
            f"Return code: {result.returncode}\n"
            f"Time: {datetime.datetime.now().isoformat()}\n\n"
            f"--- STDOUT ---\n{result.stdout}\n\n"
            f"--- STDERR ---\n{result.stderr}\n"
        )
        log_path.write_text(log_content, encoding="utf-8")

        return {
            "status": "ok" if result.returncode == 0 else "build_failed",
            "data": {
                "dry_run": False,
                **checks,
                "return_code": result.returncode,
                "log_path": str(log_path),
                "stdout": result.stdout[:1000] if result.stdout else "",
                "stderr": result.stderr[:500] if result.stderr else "",
                "message": f"构建{'成功' if result.returncode == 0 else '失败 (返回码: ' + str(result.returncode) + ')'}",
            },
        }
    except subprocess.TimeoutExpired:
        return {"status": "error", "message": "构建超时（30 分钟）", "data": checks}
    except Exception as e:
        return {"status": "error", "message": f"构建异常: {e}", "data": checks}


def check_programmer() -> dict:
    """
    检查下载线/烧录器。

    定位 efx_pgm.exe，并通过 Windows PnP/串口枚举检查 FTDI FT4232H 四通道接口。
    Efinity 2025.2 的 efx_pgm.exe 不支持 --scan，因此这里不调用该参数。
    本工具只读，不执行烧录。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    efx_pgm = cfg.efinity_bin_path() / "efx_pgm.exe"
    checks = {
        "efx_pgm_path": str(efx_pgm),
        "efx_pgm_exists": efx_pgm.is_file(),
    }

    if not checks["efx_pgm_exists"]:
        return {
            "status": "no_hardware",
            "data": {**checks, "message": "efx_pgm 未安装或路径不正确"},
        }

    # 检查 FTDI 驱动（Windows 下检查设备管理器信息）
    driver_status = "unknown"
    ftdi_ports = []
    try:
        from fpga_robot_mcp import serial_probe

        ports = serial_probe.list_ports()
        ftdi_ports = [
            p for p in ports
            if p.get("type") == "fpga_ft4232"
            or "0403:6011" in str(p.get("hwid", "")).upper()
            or "VID_0403+PID_6011" in str(p.get("hwid", "")).upper()
        ]
    except Exception:
        ports = []

    try:
        # 尝试通过 pnputil 检查
        driver_result = subprocess.run(
            ["pnputil", "/enum-devices", "/connected"],
            capture_output=True, timeout=10, encoding=_subprocess_text_encoding(), errors="replace",
        )
        driver_text = driver_result.stdout or ""
        if ftdi_ports:
            driver_status = "ft4232_ports_found"
        elif "VID_0403" in driver_text and "PID_6011" in driver_text:
            driver_status = "ft4232_usb_found"
        elif "FT4232" in driver_text or "4232" in driver_text:
            driver_status = "ft4232_found"
        elif "FT232" in driver_text:
            driver_status = "ft232_found"
        elif "oem105" in driver_text.lower() or "libusb" in driver_text.lower():
            driver_status = "libusb_installed"
        else:
            driver_status = "no_ftdi_detected"
    except Exception:
        driver_status = "ft4232_ports_found" if ftdi_ports else "check_failed"

    jtag_result = {
        "scan_supported": False,
        "scan_successful": None,
        "method": "windows_pnp_serial_probe",
        "message": (
            "Efinity 2025.2 efx_pgm.exe 不支持 --scan；"
            "当前只确认 Windows 是否识别到 FTDI/FT4232 下载接口。"
        ),
    }

    return {
        "status": "ok" if ftdi_ports else "no_hardware",
        "data": {
            **checks,
            "driver_status": driver_status,
            "ftdi_ports": ftdi_ports,
            "ftdi_port_count": len(ftdi_ports),
            "jtag_scan": jtag_result,
        },
    }


def program_bitstream(bitstream_path: str) -> dict:
    """
    烧录 bitstream 到 FPGA。

    警告: 硬件副作用操作，需通过 safety 门控 + confirm_token。

    当前 Efinity 2025.2 的实际下载 CLI 仍未完全对齐：
    - efx_pgm.exe 是 bit/hex 生成器，不是直接上板下载器；
    - Programmer GUI 使用 pgm/bin/efx_pgm/ftdi_program.py 作为 FTDI 下载后端；
    - 本机目前 Windows VCP 能看到 FT4232 COM 口，但 ftdi_program.py --list_usb 尚未看到 USB target。

    因此这里不再执行旧的 efx_pgm.exe -m ram -p 1 -b 命令，避免误报烧录能力。
    """
    cfg = _get_cfg()
    if cfg is None:
        return {"status": "error", "message": "无法加载配置"}

    bs = Path(bitstream_path)
    if not bs.is_file():
        return {"status": "error", "message": f"bitstream 文件不存在: {bitstream_path}"}

    efinity_home = cfg.efinity_home_path()
    efx_pgm = cfg.efinity_bin_path() / "efx_pgm.exe"
    setup_bat = cfg.efinity_bin_path() / "setup.bat"
    ftdi_program = efinity_home / "pgm" / "bin" / "efx_pgm" / "ftdi_program.py"

    if not efx_pgm.is_file():
        return {"status": "error", "message": f"efx_pgm 不存在: {efx_pgm}"}
    if not setup_bat.is_file():
        return {"status": "error", "message": f"Efinity setup.bat 不存在: {setup_bat}"}
    if not ftdi_program.is_file():
        return {"status": "error", "message": f"ftdi_program.py 不存在: {ftdi_program}"}

    candidate_cmd = (
        f'cmd /d /c "call {setup_bat} && python {ftdi_program} '
        f'-m jtag -b "Generic Board Profile Using FT4232" {bs}"'
    )
    preflight_cmd = (
        f'cmd /d /c "call {setup_bat} && python {ftdi_program} --list_usb"'
    )

    return {
        "status": "programmer_cli_not_aligned",
        "data": {
            "bitstream": str(bs),
            "efx_pgm_path": str(efx_pgm),
            "efx_pgm_role": "bitstream/hex generator, not the direct board programmer CLI",
            "ftdi_program_path": str(ftdi_program),
            "candidate_cli_after_usb_visible": candidate_cmd,
            "required_preflight": preflight_cmd,
            "message": (
                "已阻止旧烧录命令。请先让 ftdi_program.py --list_usb/--scan_usb 能识别 "
                "FT4232 USB target，再启用 MCP 自动烧录。当前建议用 Efinity Programmer GUI 进行首次下载。"
            ),
        },
    }


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

    # 也自动尝试搜索 outflow 中的日志
    outflow = cfg.resolve("赛方提供材料", "TJ375N529_SC431HAI2LCD_Demo_V3", "outflow")
    if outflow.is_dir():
        for f in sorted(outflow.rglob("*"), key=lambda p: p.stat().st_mtime if p.is_file() else 0, reverse=True):
            if not f.is_file():
                continue
            if f.suffix.lower() not in (".rpt", ".log"):
                continue
            # 避免重复收集已复制的文件
            dst = evidence / f.name
            if dst.exists():
                continue
            try:
                import shutil
                shutil.copy2(str(f), str(dst))
                collected.append({"src": str(f), "dst": str(dst), "size_kb": f.stat().st_size // 1024})
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
