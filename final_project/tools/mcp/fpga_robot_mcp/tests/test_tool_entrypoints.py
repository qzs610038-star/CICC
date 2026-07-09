"""
test_tool_entrypoints — 验证每个工具函数可被调用并返回 dict。

测试所有已实现的工具函数（未实现工具应返回 not_implemented 而非崩溃）。
"""

import sys
from pathlib import Path

_src = Path(__file__).resolve().parent.parent / "src"
if str(_src) not in sys.path:
    sys.path.insert(0, str(_src))

from fpga_robot_mcp import (
    serial_probe,
    board_tools,
    mycobot_tools,
    review_packet,
    efinity_tools,
    server,
)


def test_serial_probe_list_ports_callable():
    """serial_probe.list_ports() 可调用并返回列表。"""
    result = serial_probe.list_ports()
    assert isinstance(result, list)


def test_serial_probe_get_uart_summary_callable():
    """serial_probe.get_uart_summary() 可调用。"""
    result = serial_probe.get_uart_summary()
    assert isinstance(result, dict)


def test_board_tools_list_uart_candidates_callable():
    """board_tools.list_uart_candidates() 可调用。"""
    result = board_tools.list_uart_candidates()
    assert isinstance(result, dict)


def test_board_tools_check_jtag_chain_callable():
    """board_tools.check_jtag_chain() 可调用。"""
    result = board_tools.check_jtag_chain()
    assert isinstance(result, dict)


def test_board_tools_generate_uart_test_plan_callable():
    """board_tools.generate_uart_test_plan() 可调用。"""
    result = board_tools.generate_uart_test_plan()
    assert isinstance(result, dict)
    data = result.get("data", {})
    assert "test_plan_markdown" in data


def test_mycobot_tools_check_env_callable():
    """mycobot_tools.check_env() 可调用。"""
    result = mycobot_tools.check_env()
    assert isinstance(result, dict)


def test_mycobot_tools_list_ports_callable():
    """mycobot_tools.list_ports() 可调用。"""
    result = mycobot_tools.list_ports()
    assert isinstance(result, dict)


def test_mycobot_tools_plan_motion_callable():
    """mycobot_tools.plan_motion() 可调用。"""
    result = mycobot_tools.plan_motion(target="测试动作", angles=[90, 0, -90, 0, 0, 0])
    assert isinstance(result, dict)
    data = result.get("data", result)
    if "data" in result and isinstance(result["data"], dict):
        assert "markdown" in result["data"]


def test_mycobot_tools_plan_motion_empty():
    """mycobot_tools.plan_motion() 空参数应不崩溃。"""
    result = mycobot_tools.plan_motion()
    assert isinstance(result, dict)


def test_mycobot_tools_plan_motion_wrong_angles():
    """plan_motion 收到非 6 个角度不应崩溃。"""
    result = mycobot_tools.plan_motion(target="test", angles=[1, 2, 3])
    assert isinstance(result, dict)


def test_mycobot_tools_validate_connection_no_port():
    """validate_connection 无端口应返回 error。"""
    result = mycobot_tools.validate_connection("")
    assert isinstance(result, dict)
    # 可能返回 error（空端口）或 连接失败/not_implemented
    assert "status" in result


def test_mycobot_tools_read_angles_no_port():
    """read_angles 空端口应不崩溃。"""
    result = mycobot_tools.read_angles("")
    assert isinstance(result, dict)
    assert "status" in result


def test_mycobot_tools_stop_motion_no_port():
    """stop_motion 应可调用。"""
    result = mycobot_tools.stop_motion("", 1000000)
    assert isinstance(result, dict)
    assert "status" in result


def test_review_packet_generate():
    """generate_review_packet() 应返回格式化 Markdown。"""
    context = {
        "task": "验证 test 工具",
        "action_type": "只读检查",
        "config": {
            "project_root": "D:/test",
            "efinity": {"home": "D:/Efinity/2025.2", "bin": "D:/Efinity/2025.2/bin", "project_xml": "test.xml"},
            "safety": {"allow_hardware_actions": False, "require_confirm_token": True},
        },
        "safety": [],
        "risks": ["风险 1", "风险 2"],
        "questions": ["问题 1"],
    }
    result = review_packet.generate_review_packet(context)
    assert isinstance(result, dict)
    assert result["status"] == "ok"
    data = result.get("data", {})
    assert "markdown" in data
    assert "任务目标" in data["markdown"]
    assert "风险 1" in data["markdown"]
    assert "问题 1" in data["markdown"]


def test_review_packet_empty_context():
    """generate_review_packet() 空 context 应不崩溃。"""
    result = review_packet.generate_review_packet({})
    assert isinstance(result, dict)
    assert result["status"] == "ok"


def test_efinity_tools_list_artifacts_callable():
    """efinity_tools.list_artifacts() 可调用。"""
    result = efinity_tools.list_artifacts()
    assert isinstance(result, dict)


def test_efinity_tools_check_programmer_callable():
    """efinity_tools.check_programmer() 可调用。"""
    result = efinity_tools.check_programmer()
    assert isinstance(result, dict)


def test_efinity_tools_collect_logs_callable():
    """efinity_tools.collect_logs() 可调用。"""
    result = efinity_tools.collect_logs()
    assert isinstance(result, dict)


def test_server_efinity_run_build_dry_run_callable():
    """MCP 服务层构建入口 dry-run 应可调用。"""
    server._init()
    result = server.efinity_run_build(dry_run=True)
    assert isinstance(result, dict)
    assert result["status"] == "ok"
    assert result.get("data", {}).get("flow") == "compile"


def test_mycobot_tools_control_gripper_no_port():
    """control_gripper 空参数应不崩溃。"""
    result = mycobot_tools.control_gripper("", 1000000, True, 30)
    assert isinstance(result, dict)
    assert "status" in result


def test_mycobot_tools_set_rgb_no_port():
    """set_rgb 空参数应不崩溃。"""
    result = mycobot_tools.set_rgb("", 1000000, 255, 0, 0)
    assert isinstance(result, dict)
    assert "status" in result


def test_mycobot_tools_export_session_log():
    """export_session_log() 可调用。"""
    result = mycobot_tools.export_session_log()
    assert isinstance(result, dict)
    # 即使没有服务器状态，也应返回 ok 或 error
    assert "status" in result


def test_board_tools_uart_loopback_no_port():
    """uart_loopback_test 空端口应不崩溃。"""
    result = board_tools.uart_loopback_test("")
    assert isinstance(result, dict)
    assert "status" in result


def test_board_tools_generate_uart_test_plan_content():
    """generate_uart_test_plan 应包含阶段信息。"""
    result = board_tools.generate_uart_test_plan()
    data = result.get("data", {})
    md = data.get("test_plan_markdown", "")
    assert "阶段 A" in md
    assert "阶段 B" in md
    assert "阶段 C" in md
    assert "阶段 D" in md


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
