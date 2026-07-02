"""
fpga_robot_mcp.serial_probe — 串口枚举与分类。

⬅️ 留待经济模型填充。

需要实现的函数:
- list_ports() — 枚举本机所有 COM 口，按类型分类标注
- classify_port(device, description, hwid) — 单端口分类逻辑

依赖: pyserial (serial.tools.list_ports)
"""

from __future__ import annotations

from typing import Any


def list_ports() -> list[dict]:
    """
    枚举本机所有 COM 口，按类型分类标注。

    实现说明:
    1. 调用 serial.tools.list_ports.comports() 获取所有串口
    2. 对每个端口，用 classify_port() 判断类型
    3. 返回分类后的端口列表

    返回格式:
    [
        {
            "device": "COM3",
            "description": "Silicon Labs CP210x USB to UART Bridge",
            "hwid": "USB VID:PID=10C4:EA60 SER=1234",
            "type": "cp210x" | "bluetooth" | "jtag_uart" | "type_c_uart" | "unknown",
            "is_mycobot_candidate": True | False
        },
        ...
    ]

    如果 serial 模块未安装，返回:
    [{"device": "", "description": "pyserial 未安装", "type": "error", "is_mycobot_candidate": false}]

    注意事项:
    - type 字段分类规则:
      - hwid 包含 "10C4" 或 description 包含 "CP210x"/"Silicon Labs" → type="cp210x"
      - hwid 包含 "BTHENUM" 或 description 包含 "Bluetooth"/"蓝牙" → type="bluetooth"
      - description 包含 "JTAG" → type="jtag_uart"
      - 默认 → type="unknown"
    - is_mycobot_candidate = (type == "cp210x")
    - 不要因为某个端口无法读取就抛出异常，应返回带错误标记的条目
    """
    # TODO(cheap-model): 用 serial.tools.list_ports.comports() 遍历并分类
    # 参考实现:
    #   import serial.tools.list_ports
    #   ports = serial.tools.list_ports.comports()
    #   result = []
    #   for p in ports:
    #       result.append(classify_port(p.device, p.description, p.hwid))
    #   return result
    raise NotImplementedError("serial_probe.list_ports() — 需用 pyserial 枚举 COM 口并分类")


def classify_port(device: str, description: str, hwid: str) -> dict:
    """
    对单个 COM 口进行分类。

    参数:
        device: 端口名，如 "COM3"
        description: 描述，如 "Silicon Labs CP210x USB to UART Bridge"
        hwid: 硬件 ID，如 "USB VID:PID=10C4:EA60 SER=1234"

    返回:
        {
            "device": device,
            "description": description,
            "hwid": hwid,
            "type": "cp210x" | "bluetooth" | "jtag_uart" | "type_c_uart" | "unknown",
            "is_mycobot_candidate": True | False
        }

    分类规则:
    - hwid 包含 "10C4" 或 "CP210x" → cp210x
    - description 包含 "Silicon Labs" → cp210x
    - hwid 包含 "BTHENUM" 或 "蓝牙" 或 description 包含 "Bluetooth" → bluetooth
    - description 包含 "JTAG" → jtag_uart
    - description 包含 "USB Serial" → type_c_uart (开发板 Type-C UART)
    - 其他 → unknown
    """
    # TODO(cheap-model): 填充分类逻辑
    # 用 hwid.upper() 和 description 做字符串匹配
    # is_mycobot_candidate = (type == "cp210x")
    raise NotImplementedError("serial_probe.classify_port() — 需实现端口分类逻辑")


def get_uart_summary() -> dict:
    """
    返回串口摘要（供 fpga_robot_status 调用）。

    实现:
    1. 调用 list_ports()
    2. 按 type 分组计数
    3. 标注是否有 CP210x 候选

    返回:
    {
        "total_ports": 3,
        "by_type": {"cp210x": 1, "bluetooth": 2},
        "has_mycobot_candidate": True,
        "mycobot_candidates": ["COM3"],
        "ports": [...],  # list_ports() 的完整结果
    }
    """
    # TODO(cheap-model): 调用 list_ports() 并汇总
    raise NotImplementedError("serial_probe.get_uart_summary() — 需汇总 list_ports() 结果")
