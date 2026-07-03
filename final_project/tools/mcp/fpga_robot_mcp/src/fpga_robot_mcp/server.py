"""
fpga_robot_mcp.server — FastMCP 服务器入口。

所有工具通过 _safe_call() 包装注册，确保：
- 未实现工具返回友好提示 → "not_implemented"
- 安全门控拒绝 → "safety_denied"
- 异常捕获 → "error"
- 正常执行 → "ok"

启动方式：
    python -m fpga_robot_mcp.server [--verbose]
"""

from __future__ import annotations

import sys
import traceback
from typing import Any, Callable, Optional

from fpga_robot_mcp.config import load_config, hide_sensitive, FpgaRobotConfig
from fpga_robot_mcp.safety import SafetyManager, SafetyLevel

# ── 全局状态（服务生命周期内保持） ──────────────────────────

_config: Optional[FpgaRobotConfig] = None
_safety: Optional[SafetyManager] = None
_verbose: bool = False


def _init():
    """初始化配置与安全门控（服务器启动时调用）。"""
    global _config, _safety
    _config = load_config()
    _safety = SafetyManager(_config)
    if _verbose:
        print(f"[init] 配置已加载，project_root={_config.project_root}", file=sys.stderr)
        print(f"[init] 安全门控: allow_hardware={_safety.allow_hardware}", file=sys.stderr)


# ── 工具执行包装器 ──────────────────────────────────────────


class ToolError(Exception):
    """工具内部错误的基类。"""
    pass


def _safe_call(func: Callable, *args, **kwargs) -> dict:
    """
    包装工具函数调用，确保返回结构化 JSON。

    返回格式:
        {"status": "ok", "data": <返回值>}
        {"status": "not_implemented", "message": "...", "next_step": "..."}
        {"status": "safety_denied", "message": "...", "next_step": "..."}
        {"status": "error", "message": "...", "traceback": "..."}
    """
    try:
        result = func(*args, **kwargs)
        if isinstance(result, dict) and "status" in result:
            return result
        return {"status": "ok", "data": result}
    except NotImplementedError as e:
        return {
            "status": "not_implemented",
            "message": str(e) or "该工具尚未实现",
            "next_step": (
                "参阅 方案评审/002_机械臂MCP构建实施方案/"
                "机械臂MCP构建实施方案_Phase1_实施策划.md "
                "第 4 节「留待经济模型填充」"
            ),
        }
    except ToolError as e:
        return {"status": "error", "message": str(e)}
    except Exception as e:
        tb = traceback.format_exc() if _verbose else ""
        return {"status": "error", "message": str(e), "traceback": tb}


def _safety_wrapper(
    tool_name: str,
    level: SafetyLevel,
    func: Callable,
    *args,
    **kwargs,
) -> dict:
    """安全门控 + 执行包装。

    抽取 kwargs 中的 confirm_token/dry_run 用于安全门控检查；
    其余 kwargs 原样传递给底层函数。
    """
    if _safety is None:
        return {"status": "error", "message": "SafetyManager 未初始化"}

    confirm_token = kwargs.pop("confirm_token", None)
    dry_run = kwargs.pop("dry_run", False)

    allowed, msg = _safety.check_tool_allowed(
        tool_name, level, confirm_token=confirm_token, dry_run=dry_run
    )
    if not allowed:
        return {
            "status": "safety_denied",
            "message": msg,
            "next_step": "请检查安全配置或提供确认 token",
        }
    return _safe_call(func, *args, **kwargs)


# ── 导入各工具模块 ──────────────────────────────────────────

from fpga_robot_mcp import efinity_tools
from fpga_robot_mcp import serial_probe
from fpga_robot_mcp import board_tools
from fpga_robot_mcp import mycobot_tools
from fpga_robot_mcp import review_packet

# ── FastMCP 应用 ────────────────────────────────────────────

from mcp.server.fastmcp import FastMCP

mcp = FastMCP(
    "fpga_robot_mcp",
)

# ══════════════════════════════════════════════════════════════
# 工具注册
# ══════════════════════════════════════════════════════════════


# ── 1. 项目与环境工具 ────────────────────────────────────


@mcp.tool(
    description="汇总当前项目环境状态：Python 版本、依赖、Efinity 路径、串口候选、安全配置。"
)
def fpga_robot_status() -> dict:
    """汇总环境状态。"""
    def _impl():
        import importlib.util
        import os
        import sys as _sys

        if _config is None:
            return {"error": "未初始化"}

        # Python 环境
        py_info = {
            "version": f"{_sys.version_info.major}.{_sys.version_info.minor}.{_sys.version_info.micro}",
            "executable": _sys.executable,
        }

        # 依赖检查
        deps = {}
        for mod_name in ["mcp", "serial", "pydantic", "pymycobot"]:
            spec = importlib.util.find_spec(mod_name)
            deps[mod_name] = "installed" if spec else "missing"

        # Efinity
        efx_result = efinity_tools.locate_toolchain()
        efx_ok = efx_result.get("status") == "ok"

        # RISC-V IDE / CPU toolchain
        riscv_home = _config.riscv_ide.home
        riscv_toolchain_bin = _config.riscv_ide.toolchain_bin
        riscv_openocd = _config.riscv_ide.openocd
        riscv_build_tools = _config.riscv_ide.build_tools
        riscv_checks = {
            "home_dir": os.path.isdir(riscv_home),
            "toolchain_bin": os.path.isdir(riscv_toolchain_bin),
            "gcc": os.path.isfile(os.path.join(riscv_toolchain_bin, "riscv-none-embed-gcc.exe")),
            "gdb": os.path.isfile(os.path.join(riscv_toolchain_bin, "riscv-none-embed-gdb.exe")),
            "openocd": os.path.isfile(riscv_openocd),
            "make": os.path.isfile(os.path.join(riscv_build_tools, "make.exe")),
        }
        riscv_ok = all(riscv_checks.values())

        # 串口 — 调用 serial_probe（可能未实现）
        ports_result = _safe_call(serial_probe.list_ports)

        # myCobot — 调用 mycobot_tools（可能未实现）
        mycobot_result = _safe_call(mycobot_tools.check_env)

        return {
            "project_root": _config.project_root,
            "python": py_info,
            "dependencies": deps,
            "efinity": {
                "installed": efx_ok,
                "home": _config.efinity.home if efx_ok else None,
                "detail": efx_result.get("data", efx_result.get("message", "")),
            },
            "riscv_ide": {
                "installed": riscv_ok,
                "home": riscv_home if riscv_checks["home_dir"] else None,
                "checks": riscv_checks,
                "toolchain_bin": riscv_toolchain_bin,
                "openocd": riscv_openocd,
                "build_tools": riscv_build_tools,
            },
            "serial_ports": ports_result.get("data", ports_result),
            "mycobot": mycobot_result.get("data", mycobot_result),
            "safety": {
                "allow_hardware_actions": _config.safety.allow_hardware_actions,
                "require_confirm_token": _config.safety.require_confirm_token,
            },
        }

    return _safe_call(_impl)


@mcp.tool(
    description="返回当前 MCP 配置（隐藏敏感字段如 token、密码）。"
)
def fpga_robot_get_config() -> dict:
    """返回当前配置。"""
    def _impl():
        if _config is None:
            return {"error": "未初始化"}
        return hide_sensitive(_config)
    return _safe_call(_impl)


@mcp.tool(
    description="返回指定工具的 PowerShell 等效手动操作步骤。"
)
def fpga_robot_manual_fallback(tool_name: str = "") -> dict:
    """返回手动等效操作指南。"""
    fallback_map = {
        "fpga_robot_status": "python --version; Get-Command efx_run -ErrorAction SilentlyContinue; "
                             "python -c \"import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]\"",
        "efinity_locate_toolchain": "Get-ChildItem \"D:\\Efinity\\2025.2\\bin\" -Filter \"efx_*\"",
        "efinity_check_install_prereq": "Get-Command 7z,unrar,winrar -ErrorAction SilentlyContinue; "
                                         "Test-Path \"D:\\Efinity\\2025.2\"",
        "efinity_check_project": "Test-Path \"赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/mem_test.xml\"; "
                                  "Test-Path \"赛方提供材料/TJ375N529_SC431HAI2LCD_Demo_V3/constrain.sdc\"",
        "board_list_uart_candidates": "python -c \"import serial.tools.list_ports as p; [print(x.device, x.description, x.hwid) for x in p.comports()]\"",
        "mycobot280_check_env": "python -c \"import importlib.util; print(bool(importlib.util.find_spec('pymycobot')))\"",
    }
    if tool_name:
        cmd = fallback_map.get(tool_name)
        if cmd:
            return {"tool": tool_name, "powershell": cmd}
        return {"tool": tool_name, "message": f"未找到 {tool_name} 的手动操作指南"}
    return {"available_tools": list(fallback_map.keys()), "total": len(fallback_map)}


# ── 2. Efinity / 烧录工具 ──────────────────────────────────


@mcp.tool(description="搜索 Efinity 安装路径、CLI 可执行文件、版本信息。")
def efinity_locate_toolchain() -> dict:
    return _safe_call(efinity_tools.locate_toolchain)


@mcp.tool(description="检查 RAR 解压工具、安装包、补丁脚本、磁盘空间和默认安装路径。")
def efinity_check_install_prereq() -> dict:
    return _safe_call(efinity_tools.check_install_prereq)


@mcp.tool(description="校验 mem_test.xml、constrain.sdc、器件型号、顶层模块和输出目录。")
def efinity_check_project() -> dict:
    return _safe_call(efinity_tools.check_project)


@mcp.tool(description="列出 outflow 构建产物（bitstream、report、log），返回时间戳和大小。")
def efinity_list_artifacts(limit: int = 30, offset: int = 0) -> dict:
    return _safe_call(efinity_tools.list_artifacts, limit, offset)


@mcp.tool(description="运行综合/布局布线流程。dry_run=true 时只做预检查。")
def efinity_run_build(
    project_xml: str = "",
    dry_run: bool = True,
) -> dict:
    return _safety_wrapper(
        "efinity_run_build", SafetyLevel.WRITE_FILE,
        efinity_tools.run_build, project_xml, dry_run,
        dry_run=dry_run,
    )


@mcp.tool(description="检查下载线、驱动和可用烧录器信息；不执行烧录。")
def efinity_check_programmer() -> dict:
    return _safe_call(efinity_tools.check_programmer)


@mcp.tool(
    description="调用 Efinity 烧录工具。必须提供 dry_run=false、目标 bitstream 和确认 token。"
)
def efinity_program_bitstream(
    bitstream_path: str,
    confirm_token: str = "",
    dry_run: bool = False,
) -> dict:
    return _safety_wrapper(
        "efinity_program_bitstream", SafetyLevel.HARDWARE_SIDE_EFFECT,
        efinity_tools.program_bitstream, bitstream_path,
        confirm_token=confirm_token, dry_run=dry_run,
    )


@mcp.tool(description="收集构建和烧录日志到证据目录。")
def efinity_collect_logs(log_dir: str = "") -> dict:
    return _safe_call(efinity_tools.collect_logs, log_dir)


# ── 3. 开发板 / CPU / 串口工具 ─────────────────────────────


@mcp.tool(description="枚举 COM 口，按 CP210x、JTAG UART、Type-C UART、蓝牙虚拟串口分组。")
def board_list_uart_candidates() -> dict:
    return _safe_call(board_tools.list_uart_candidates)


@mcp.tool(description="通过 Windows PnP/串口枚举检查 FT4232/JTAG 下载接口可见性，不烧录。")
def board_check_jtag_chain() -> dict:
    return _safe_call(board_tools.check_jtag_chain)


@mcp.tool(description="在明确选择的 UART 上执行回环测试帧。不连接机械臂、不触发动作。")
def board_uart_loopback_test(
    port: str,
    baudrate: int = 115200,
    confirm_token: str = "",
) -> dict:
    return _safety_wrapper(
        "board_uart_loopback_test", SafetyLevel.HARDWARE_SIDE_EFFECT,
        board_tools.uart_loopback_test, port, baudrate,
        confirm_token=confirm_token,
    )


# ── 4. myCobot 280 工具 ────────────────────────────────────


@mcp.tool(description="检查 Python、pyserial、pymycobot、CP210x 驱动、COM 口。")
def mycobot280_check_env() -> dict:
    return _safe_call(mycobot_tools.check_env)


@mcp.tool(description="列出串口并标注是否像 CP210x/myCobot 候选。")
def mycobot280_list_ports() -> dict:
    return _safe_call(mycobot_tools.list_ports)


@mcp.tool(description="在用户确认已连接后尝试打开串口并做只读握手和状态读取。")
def mycobot280_validate_connection(
    port: str,
    baudrate: int = 1000000,
) -> dict:
    return _safe_call(mycobot_tools.validate_connection, port, baudrate)


@mcp.tool(description="读取 myCobot 280 的 6 个关节角度（只读，不触发运动）。")
def mycobot280_read_angles(
    port: str,
    baudrate: int = 1000000,
) -> dict:
    return _safe_call(mycobot_tools.read_angles, port, baudrate)


@mcp.tool(description="读取 myCobot 280 当前坐标（只读，不触发运动）。")
def mycobot280_read_coords(
    port: str,
    baudrate: int = 1000000,
) -> dict:
    return _safe_call(mycobot_tools.read_coords, port, baudrate)


@mcp.tool(description="设置 myCobot 280 RGB 灯板颜色（低风险非运动测试）。")
def mycobot280_set_rgb(
    port: str,
    r: int = 0,
    g: int = 255,
    b: int = 0,
    baudrate: int = 1000000,
    confirm_token: str = "",
) -> dict:
    return _safety_wrapper(
        "mycobot280_set_rgb", SafetyLevel.HARDWARE_SIDE_EFFECT,
        mycobot_tools.set_rgb, port, baudrate, r, g, b,
        confirm_token=confirm_token,
    )


@mcp.tool(description="根据目标动作生成动作摘要、角度范围、速度、风险提示（只读，不执行）。")
def mycobot280_plan_motion(
    target: str = "",
    angles: Optional[list[float]] = None,
) -> dict:
    return _safe_call(mycobot_tools.plan_motion, target, angles or [])


@mcp.tool(description="执行极小幅关节/坐标动作。必须提供确认 token 和速度上限。")
def mycobot280_execute_motion(
    port: str,
    angles: list[float],
    speed: int = 30,
    baudrate: int = 1000000,
    confirm_token: str = "",
) -> dict:
    return _safety_wrapper(
        "mycobot280_execute_motion", SafetyLevel.HARDWARE_SIDE_EFFECT,
        mycobot_tools.execute_motion, port, baudrate, angles, speed,
        confirm_token=confirm_token,
    )


@mcp.tool(description="控制 myCobot 280 夹爪。必须提供确认 token 和速度上限。")
def mycobot280_control_gripper(
    port: str,
    gripper_open: bool = True,
    speed: int = 30,
    baudrate: int = 1000000,
    confirm_token: str = "",
) -> dict:
    return _safety_wrapper(
        "mycobot280_control_gripper", SafetyLevel.HARDWARE_SIDE_EFFECT,
        mycobot_tools.control_gripper, port, baudrate, gripper_open, speed,
        confirm_token=confirm_token,
    )


@mcp.tool(description="发送停止/释放相关命令。允许高优先级调用。")
def mycobot280_stop(
    port: str,
    baudrate: int = 1000000,
    confirm_token: str = "",
) -> dict:
    return _safety_wrapper(
        "mycobot280_stop", SafetyLevel.HARDWARE_SIDE_EFFECT,
        mycobot_tools.stop_motion, port, baudrate,
        confirm_token=confirm_token,
    )


@mcp.tool(description="导出本次连接、动作摘要、返回值和错误记录。")
def mycobot280_export_session_log() -> dict:
    return _safe_call(mycobot_tools.export_session_log)


# ── 5. Review Packet 工具 ──────────────────────────────────


@mcp.tool(description="生成 Review Packet Markdown。正式评审包写方案评审/ 目录，日常证据写 final_project/docs/evidence/。")
def fpga_robot_write_review_packet(
    task: str = "",
    action_type: str = "",
    risks: Optional[list[str]] = None,
    questions: Optional[list[str]] = None,
) -> dict:
    return _safe_call(
        lambda: review_packet.generate_review_packet({
            "task": task,
            "action_type": action_type,
            "config": hide_sensitive(_config) if _config else {},
            "safety": _safety.get_recent_audit(10) if _safety else [],
            "risks": risks or [],
            "questions": questions or [],
        })
    )


# ── 入口 ────────────────────────────────────────────────────


def main():
    global _verbose
    if "--verbose" in sys.argv or "-v" in sys.argv:
        _verbose = True
    _init()
    tool_count = len(mcp._tool_manager.list_tools())
    print(f"[fpga_robot_mcp] 服务器启动，{tool_count} 个工具已注册", file=sys.stderr)
    mcp.run()


if __name__ == "__main__":
    main()
