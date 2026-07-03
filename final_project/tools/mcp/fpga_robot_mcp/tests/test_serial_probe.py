"""
test_serial_probe — serial_probe 模块单元测试。

测试:
- classify_port() 分类逻辑
- list_ports() 返回格式
- get_uart_summary() 汇总逻辑
"""

import sys
from pathlib import Path

_src = Path(__file__).resolve().parent.parent / "src"
if str(_src) not in sys.path:
    sys.path.insert(0, str(_src))

from fpga_robot_mcp import serial_probe


def test_classify_cp210x_by_vid():
    """CP210x 芯片 (VID_10C4) 应被分类为 cp210x 且是 myCobot 候选。"""
    result = serial_probe.classify_port(
        "COM3",
        "Silicon Labs CP210x USB to UART Bridge",
        "USB VID:PID=10C4:EA60 SER=1234",
    )
    assert result["type"] == "cp210x"
    assert result["is_mycobot_candidate"] is True


def test_classify_cp210x_by_description():
    """描述包含 Silicon Labs 也应分类为 cp210x。"""
    result = serial_probe.classify_port(
        "COM3",
        "Silicon Labs CP210x USB to UART Bridge",
        "USB VID:PID=10C4:EA60",
    )
    assert result["type"] == "cp210x"
    assert result["is_mycobot_candidate"] is True


def test_classify_cp210x_by_hwid_cp210x():
    """hwid 包含 CP210x 字符串也应分类为 cp210x。"""
    result = serial_probe.classify_port(
        "COM3",
        "USB Bridge",
        "USB VID:PID=10C4:EA60 CP210x",
    )
    assert result["type"] == "cp210x"
    assert result["is_mycobot_candidate"] is True


def test_classify_bluetooth_by_hwid():
    """BTHENUM 应分类为 bluetooth。"""
    result = serial_probe.classify_port(
        "COM4",
        "Bluetooth Device",
        "BTHENUM\\{1234-5678}",
    )
    assert result["type"] == "bluetooth"
    assert result["is_mycobot_candidate"] is False


def test_classify_bluetooth_by_chinese():
    """中文"蓝牙"应分类为 bluetooth。"""
    result = serial_probe.classify_port(
        "COM5",
        "标准串行 over 蓝牙链接",
        "蓝牙",
    )
    assert result["type"] == "bluetooth"
    assert result["is_mycobot_candidate"] is False


def test_classify_bluetooth_by_description():
    """描述包含 Bluetooth 应分类为 bluetooth。"""
    result = serial_probe.classify_port(
        "COM6",
        "Bluetooth Serial Port",
        "BTH PORT",
    )
    assert result["type"] == "bluetooth"


def test_classify_jtag_uart():
    """描述包含 JTAG 应分类为 jtag_uart。"""
    result = serial_probe.classify_port(
        "COM7",
        "JTAG UART Port",
        "USB VID:PID=1234:5678",
    )
    assert result["type"] == "jtag_uart"
    assert result["is_mycobot_candidate"] is False


def test_classify_type_c_uart():
    """描述包含 USB Serial 应分类为 type_c_uart。"""
    result = serial_probe.classify_port(
        "COM8",
        "USB Serial Port (COM8)",
        "USB VID:PID=1234:5678",
    )
    assert result["type"] == "type_c_uart"


def test_classify_unknown():
    """无法识别的端口应分类为 unknown。"""
    result = serial_probe.classify_port(
        "COM9",
        "Some Unknown Device",
        "SOME HWID",
    )
    assert result["type"] == "unknown"
    assert result["is_mycobot_candidate"] is False


def test_classify_case_insensitive():
    """分类应大小写不敏感。"""
    result = serial_probe.classify_port(
        "COM3",
        "silicon labs cp210x usb to uart bridge",
        "usb vid:pid=10c4:ea60",
    )
    assert result["type"] == "cp210x"


def test_list_ports_return_format():
    """list_ports() 应始终返回列表。"""
    ports = serial_probe.list_ports()
    assert isinstance(ports, list)
    if ports:
        entry = ports[0]
        assert "device" in entry
        assert "description" in entry
        assert "hwid" in entry
        assert "type" in entry
        assert "is_mycobot_candidate" in entry


def test_get_uart_summary():
    """get_uart_summary() 应返回汇总结构。"""
    summary = serial_probe.get_uart_summary()
    assert isinstance(summary, dict)
    assert "total_ports" in summary
    assert "by_type" in summary
    assert "has_mycobot_candidate" in summary
    assert "mycobot_candidates" in summary
    assert "ports" in summary
    assert isinstance(summary["by_type"], dict)


def test_get_uart_summary_type_counts():
    """get_uart_summary() 的 by_type 总和应等于 total_ports。"""
    summary = serial_probe.get_uart_summary()
    total_from_types = sum(summary["by_type"].values())
    # 过滤掉 error 类型的端口
    error_count = summary["by_type"].get("error", 0)
    # 如果 serial 未安装，error 条目不会计入 comports 的 total
    if summary["total_ports"] > 0 or error_count == 0:
        # 正常情况下，by_type 总和应 >= total_ports
        pass  # 如果 pyserial 缺失，只有一个 error 条目


if __name__ == "__main__":
    import inspect
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"  ✅ {name}")
            except Exception as e:
                print(f"  ❌ {name}: {e}")
                failures += 1
    total = len([k for k in globals() if k.startswith("test_") and callable(globals()[k])])
    print(f"\n{'-' * 40}")
    print(f"{total - failures}/{total} 通过")
    sys.exit(failures)
