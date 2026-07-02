"""
fpga_robot_mcp.board_tools — 开发板 UART/JTAG/接口契约工具。

⬅️ 留待经济模型填充。

需要实现的函数:
- list_uart_candidates() — 枚举开发板 UART 候选项
- check_jtag_chain() — 探测 JTAG 链
- uart_loopback_test() — UART 回环测试（低风险硬件操作）
- generate_uart_test_plan() — 生成 UART 桥接测试计划文档
"""

from __future__ import annotations

from typing import Any


def list_uart_candidates() -> dict:
    """
    枚举开发板 UART 候选项。

    实现说明:
    1. 尝试导入 serial_probe 模块并调用 list_ports()
    2. 过滤掉蓝牙端口（type != "bluetooth"）
    3. 按类型分组列出

    返回格式:
    {
        "all_ports": [...],  # 完整端口列表（调用 serial_probe.list_ports()）
        "board_candidates": [...],  # 过滤蓝牙后的候选项
        "total": 3,
        "board_uart_count": 1,
    }

    如果 serial_probe 未实现，返回友好提示。
    """
    # TODO(cheap-model): 调用 serial_probe.list_ports() 并过滤
    raise NotImplementedError("board_tools.list_uart_candidates() — 需调用 serial_probe.list_ports()")


def check_jtag_chain() -> dict:
    """
    探测 JTAG 链上的设备。

    实现说明（需要 efx_pgm --scan 命令可用）:
    1. 定位 cfg.efinity_bin_path() / "efx_pgm.exe"
    2. 检查 efx_pgm 文件是否存在
    3. 如果存在，尝试 subprocess.run([efx_pgm, "--scan"], capture_output=True, timeout=15)
    4. 解析 stdout 中 JTAG 链设备信息
    5. 如果 efx_pgm 不存在或扫描失败，返回友好错误

    返回格式:
    {
        "efx_pgm_exists": true,
        "scan_successful": true,
        "jtag_devices": [
            {"position": 0, "idcode": "0x01234567", "manufacturer": "Efinix"}
        ],
        "raw_output": "...",  # scan 原始输出（前 500 字符）
    }

    如果 efx_pgm 不存在:
    {
        "efx_pgm_exists": false,
        "message": "efx_pgm 未安装或路径不正确",
        "next_step": "请确认 Efinity 安装路径和 FT4232 驱动"
    }

    注意:
    - 本工具只读，不烧录
    - 超时 15 秒
    - 首次调用可能因驱动未就绪失败，这是正常的
    """
    # TODO(cheap-model): 定位 efx_pgm.exe，尝试 subprocess.run 扫描
    raise NotImplementedError("board_tools.check_jtag_chain() — 需调用 efx_pgm --scan")


def uart_loopback_test(port: str, baudrate: int = 115200) -> dict:
    """
    在指定 UART 上执行回环测试帧。

    警告: 本工具需 safety 门控（HARDWARE_SIDE_EFFECT），但不连接机械臂。

    实现说明:
    1. 用 pyserial 打开指定端口（timeout=2）
    2. 发送测试帧（如 b"FPGA_ROBOT_MCP_LOOPBACK_TEST_\\n"）
    3. 读取回环数据
    4. 比较发送和接收数据
    5. 关闭端口
    6. 返回测试结果

    返回格式:
    {
        "port": "COM3",
        "baudrate": 115200,
        "test_passed": true,
        "bytes_sent": 32,
        "bytes_received": 32,
        "match": true,
        "duration_ms": 15.3,
    }

    注意:
    - 必须先物理连接 UART 回环线（TX→RX）才能成功
    - 超时不算失败，而是提示"可能需要回环线"
    - 不连接机械臂
    """
    # TODO(cheap-model): 用 pyserial 发送测试帧并验证回环
    raise NotImplementedError("board_tools.uart_loopback_test() — 需用 pyserial 执行回环测试")


def generate_uart_test_plan() -> dict:
    """
    生成板上 CPU 到 myCobot 串口桥接测试计划（只生成文档，不操作硬件）。

    实现说明:
    1. 读取项目当前状态
    2. 生成 Markdown 格式测试计划
    3. 测试计划应包含:
       - 测试目的
       - 硬件连接（UART 引脚、波特率、电平）
       - 测试步骤（回环→单字节→多字节→协议帧→带外数据）
       - 预期结果
       - 安全注意事项

    返回:
    {"test_plan_markdown": "..."}
    """
    # TODO(cheap-model): 生成格式化的测试计划 Markdown
    raise NotImplementedError("board_tools.generate_uart_test_plan() — 需生成 UART 测试计划文档")
