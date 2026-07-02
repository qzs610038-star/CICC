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

import os
import shutil
import subprocess  # noqa: F401 — 留待经济模型使用
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

    # 2. 检查 CLI 可执行文件
    if checks["bin_dir"]:
        for tool in ["efx_map.exe", "efx_pgm.exe", "efx_pnr.exe", "efx_run.bat", "efx_simulate.exe"]:
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

    all_found = all(v for k, v in checks.items() if k.startswith("efx_") or k in ("home_dir", "bin_dir"))

    return {
        "status": "ok" if all_found else ("partial" if checks.get("home_dir") else "missing"),
        "data": {
            "efinity_home": efinity_home,
            "efinity_bin": efinity_bin,
            "checks": checks,
            "found_tools": found_tools,
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

    需要实现:
    1. 定位 outflow 目录（cfg.resolve("赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/outflow")）
    2. 递归遍历 *.bit、*.rpt、*.log、*.txt 等文件
    3. 按修改时间倒序排列
    4. 应用 limit/offset 分页
    5. 返回每个文件的: path(相对), size_bytes, last_modified, type(bit/rpt/log)

    返回格式示例:
    {
        "status": "ok",
        "data": {
            "total": 42,
            "artifacts": [
                {
                    "path": "top.bit",
                    "size_bytes": 123456,
                    "last_modified": "2026-07-01T10:30:00",
                    "type": "bit"
                },
                ...
            ]
        }
    }
    """
    raise NotImplementedError("efinity_tools.list_artifacts() — 需遍历 outflow 目录并列出产物")


def run_build(project_xml: str = "", dry_run: bool = True) -> dict:
    """
    运行 Efinity 综合/布局布线。

    需要实现:
    1. 解析 project_xml 路径（空则用 cfg.efinity_project_path()）
    2. 定位 efx_run.bat (cfg.efinity_bin_path() / "efx_run.bat")
    3. dry_run=True: 只检查文件存在性、工程可读性，不启动子进程
    4. dry_run=False: 调用 subprocess.run([efx_run, "-f", project_xml], ...)
       - 注意：首次运行前需要 Efinity license（如需要）
       - 设置超时（默认 30 分钟）
       - 捕获 stdout/stderr
       - 返回构建日志路径

    返回格式示例:
    {
        "status": "ok",
        "data": {
            "dry_run": true,
            "project_xml": "...",
            "efx_run": "...",
            "checks": {...},  # dry_run 时返回预检查结果
            "log_path": "...",  # dry_run=false 时返回
            "return_code": 0,  # dry_run=false 时返回
        }
    }
    """
    raise NotImplementedError("efinity_tools.run_build() — 需调用 efx_run.bat 子进程；dry_run 做预检查")


def check_programmer() -> dict:
    """
    检查下载线/烧录器。

    需要实现:
    1. 定位 efx_pgm.exe (cfg.efinity_bin_path() / "efx_pgm.exe")
    2. 尝试 subprocess.run([efx_pgm, "--scan"], capture_output=True, timeout=10)
    3. 解析输出，查找连接的 JTAG 设备
    4. 也检查 FTDI / FT4232 驱动状态

    注意: 本工具只读，不执行烧录。

    返回格式示例:
    {
        "status": "ok" | "no_hardware",
        "data": {
            "efx_pgm_path": "...",
            "efx_pgm_exists": true,
            "jtag_devices": [...],  # 扫描到的设备
            "driver_status": "installed" | "missing",
        }
    }
    """
    raise NotImplementedError("efinity_tools.check_programmer() — 需调用 efx_pgm --scan 并解析输出")


def program_bitstream(bitstream_path: str) -> dict:
    """
    烧录 bitstream 到 FPGA。

    警告: 本工具是硬件副作用操作，需通过 safety 门控 + confirm_token。

    需要实现:
    1. 确认 bitstream_path 存在（.bit 或 .svf 文件）
    2. 调用 efx_pgm [options] -m ram -p 1 -b <bitstream_path>
    3. 捕获 stdout/stderr
    4. 验证烧录结果（检查返回码和日志中的成功标志）

    返回格式示例:
    {
        "status": "ok" | "error",
        "data": {
            "bitstream": "...",
            "command": "...",
            "return_code": 0,
            "log": "...",
            "duration_seconds": 12.3,
        }
    }
    """
    raise NotImplementedError("efinity_tools.program_bitstream() — 硬件副作用操作，需 safety 门控")


def collect_logs(log_dir: str = "") -> dict:
    """
    收集构建/烧录日志到证据目录。

    需要实现:
    1. 定位证据目录（cfg.evidence_path()，确保存在）
    2. 如果 log_dir 为空，搜索常见日志位置:
       - outflow/*.rpt
       - outflow/*.log
       - 工程目录下的 build_log/
    3. 复制或汇总到证据目录
    4. 返回复制的文件列表
    """
    raise NotImplementedError("efinity_tools.collect_logs() — 需收集日志文件到证据目录")
