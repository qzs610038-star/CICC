"""
fpga_robot_mcp.serial_probe — 串口枚举与分类。

实现:
- list_ports() — 枚举本机所有 COM 口，按类型分类标注
- classify_port() — 单端口分类逻辑
- get_uart_summary() — 汇总串口状态供 fpga_robot_status 调用

依赖: pyserial (serial.tools.list_ports) — 可选
"""

from __future__ import annotations

from typing import Any


def classify_port(device: str, description: str, hwid: str) -> dict:
    """
    对单个 COM 口进行分类。

    分类规则:
    - hwid 包含 "10C4" 或 "CP210x" → cp210x
    - description 包含 "Silicon Labs" → cp210x
    - hwid 包含 "BTHENUM" 或 "蓝牙" 或 description 包含 "Bluetooth" → bluetooth
    - description 包含 "JTAG" → jtag_uart
    - hwid 包含 "0403:6011" → fpga_ft4232 (Efinix/TJ375 板卡四通道接口候选)
    - description 包含 "USB Serial" → type_c_uart (开发板 Type-C UART)
    - 其他 → unknown
    """
    hwid_upper = hwid.upper()
    desc_lower = description.lower()

    port_type = "unknown"
    is_mycobot_candidate = False
    is_board_candidate = False

    # FTDI FT4232H / 0403:6011，多数 Efinix/TJ375 板卡会枚举为四路 USB Serial。
    if "0403:6011" in hwid_upper or "VID_0403+PID_6011" in hwid_upper:
        port_type = "fpga_ft4232"
        is_board_candidate = True

    # CP210x / myCobot 候选
    elif "10C4" in hwid_upper or "CP210X" in hwid_upper:
        port_type = "cp210x"
        is_mycobot_candidate = True
    elif "silicon labs" in desc_lower:
        port_type = "cp210x"
        is_mycobot_candidate = True

    # 蓝牙
    elif "BTHENUM" in hwid_upper or "蓝牙" in hwid_upper:
        port_type = "bluetooth"
    elif "bluetooth" in desc_lower:
        port_type = "bluetooth"

    # JTAG UART
    elif "jtag" in desc_lower:
        port_type = "jtag_uart"

    # Type-C UART
    elif "usb serial" in desc_lower:
        port_type = "type_c_uart"

    return {
        "device": device,
        "description": description,
        "hwid": hwid,
        "type": port_type,
        "is_mycobot_candidate": is_mycobot_candidate,
        "is_board_candidate": is_board_candidate,
    }


def list_ports() -> list[dict]:
    """
    枚举本机所有 COM 口，按类型分类标注。

    返回格式:
    [
        {
            "device": "COM3",
            "description": "Silicon Labs CP210x USB to UART Bridge",
            "hwid": "USB VID:PID=10C4:EA60 SER=1234",
            "type": "cp210x" | "bluetooth" | "jtag_uart" | "type_c_uart" | "unknown",
            "is_mycobot_candidate": True | False
        },
    ]

    如果 serial 模块未安装，返回错误条目。
    """
    try:
        import serial.tools.list_ports
    except ImportError:
        return [{
            "device": "",
            "description": "pyserial 未安装",
            "hwid": "",
            "type": "error",
            "is_mycobot_candidate": False,
            "is_board_candidate": False,
        }]

    try:
        ports = serial.tools.list_ports.comports()
    except Exception as e:
        return [{
            "device": "",
            "description": f"串口枚举失败: {e}",
            "hwid": "",
            "type": "error",
            "is_mycobot_candidate": False,
            "is_board_candidate": False,
        }]

    result = []
    for p in ports:
        try:
            entry = classify_port(p.device, p.description, p.hwid)
            result.append(entry)
        except Exception:
            result.append({
                "device": getattr(p, "device", "?"),
                "description": getattr(p, "description", "?"),
                "hwid": getattr(p, "hwid", "?"),
                "type": "unknown",
                "is_mycobot_candidate": False,
                "is_board_candidate": False,
            })
    return result


def get_uart_summary() -> dict:
    """
    返回串口摘要（供 fpga_robot_status 调用）。

    返回:
    {
        "total_ports": 3,
        "by_type": {"cp210x": 1, "bluetooth": 2},
        "has_mycobot_candidate": True,
        "mycobot_candidates": ["COM3"],
        "ports": [...],
    }
    """
    ports = list_ports()
    by_type: dict[str, int] = {}
    candidates: list[str] = []
    board_candidates: list[str] = []

    for p in ports:
        t = p.get("type", "unknown")
        by_type[t] = by_type.get(t, 0) + 1
        if p.get("is_mycobot_candidate"):
            candidates.append(p["device"])
        if p.get("is_board_candidate"):
            board_candidates.append(p["device"])

    return {
        "total_ports": len(ports),
        "by_type": by_type,
        "has_mycobot_candidate": len(candidates) > 0,
        "mycobot_candidates": candidates,
        "has_board_candidate": len(board_candidates) > 0,
        "board_candidates": board_candidates,
        "ports": ports,
    }
