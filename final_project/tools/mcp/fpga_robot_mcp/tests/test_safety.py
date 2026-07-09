"""
test_safety — 安全门控与 confirm token 测试。
"""

import sys
from pathlib import Path

_src = Path(__file__).resolve().parent.parent / "src"
if str(_src) not in sys.path:
    sys.path.insert(0, str(_src))

from fpga_robot_mcp.safety import SafetyManager, SafetyLevel
from fpga_robot_mcp.config import get_default_config


def _make_manager(allow_hardware: bool = False):
    cfg = get_default_config()
    cfg.safety.allow_hardware_actions = allow_hardware
    return SafetyManager(cfg)


def test_readonly_default_allowed():
    """只读操作应默认允许。"""
    sm = _make_manager()
    allowed, msg = sm.check_tool_allowed("fpga_robot_status", SafetyLevel.READONLY)
    assert allowed is True


def test_write_file_denied_without_hardware():
    """写文件操作在 allow_hardware_actions=false 时应拒绝。"""
    sm = _make_manager(allow_hardware=False)
    allowed, msg = sm.check_tool_allowed("efinity_run_build", SafetyLevel.WRITE_FILE)
    assert allowed is False
    assert "dry_run" in msg or "allow_hardware" in msg


def test_write_file_dryrun_allowed():
    """写文件操作在 dry_run=true 时应允许。"""
    sm = _make_manager(allow_hardware=False)
    allowed, msg = sm.check_tool_allowed("efinity_run_build", SafetyLevel.WRITE_FILE, dry_run=True)
    assert allowed is True


def test_hardware_dryrun_allowed_without_config():
    """硬件副作用 dry-run 只生成计划，应默认允许。"""
    sm = _make_manager(allow_hardware=False)
    allowed, msg = sm.check_tool_allowed(
        "efinity_program_bitstream", SafetyLevel.HARDWARE_SIDE_EFFECT, dry_run=True
    )
    assert allowed is True
    assert "dry-run" in msg


def test_hardware_denied_without_config():
    """硬件副作用操作在未配置时应拒绝。"""
    sm = _make_manager(allow_hardware=False)
    allowed, msg = sm.check_tool_allowed(
        "efinity_program_bitstream", SafetyLevel.HARDWARE_SIDE_EFFECT
    )
    assert allowed is False


def test_hardware_denied_without_token():
    """硬件副作用操作在配置允许但无 token 时应拒绝。"""
    sm = _make_manager(allow_hardware=True)
    allowed, msg = sm.check_tool_allowed(
        "mycobot280_execute_motion", SafetyLevel.HARDWARE_SIDE_EFFECT
    )
    assert allowed is False
    assert "token" in msg.lower()


def test_hardware_allowed_with_token():
    """硬件副作用操作在配置允许 + 有效 token 时应允许。"""
    sm = _make_manager(allow_hardware=True)
    token = sm.generate_confirm_token("MYCOBOT280_SAFE")
    allowed, msg = sm.check_tool_allowed(
        "mycobot280_execute_motion", SafetyLevel.HARDWARE_SIDE_EFFECT,
        confirm_token=token,
    )
    assert allowed is True
    logs = sm.get_recent_audit()
    assert token not in str(logs)
    assert "present_redacted" in str(logs)


def test_bad_token_rejected():
    """格式不匹配的 token 应被拒绝。"""
    sm = _make_manager(allow_hardware=True)
    allowed, msg = sm.check_tool_allowed(
        "mycobot280_execute_motion", SafetyLevel.HARDWARE_SIDE_EFFECT,
        confirm_token="BAD_TOKEN",
    )
    assert allowed is False


def test_generate_confirm_token():
    """生成 token 格式应正确。"""
    token = SafetyManager.generate_confirm_token("EFINITY_PROGRAM")
    assert token.startswith("I_CONFIRM_EFINITY_PROGRAM_")
    assert len(token) == len("I_CONFIRM_EFINITY_PROGRAM_") + 8  # 8 位日期
    # 应包含今日日期
    import datetime
    today = datetime.date.today().strftime("%Y%m%d")
    assert token.endswith(today)


def test_validate_confirm_token():
    """validate_confirm_token 应匹配正确格式。"""
    token = "I_CONFIRM_MYCOBOT280_SAFE_20260702"
    assert SafetyManager.validate_confirm_token(token, "MYCOBOT280_SAFE") is True
    assert SafetyManager.validate_confirm_token("BAD", "MYCOBOT280_SAFE") is False
    assert SafetyManager.validate_confirm_token("", "MYCOBOT280_SAFE") is False


def test_audit_log_entries():
    """审计日志应有条目记录。"""
    sm = _make_manager()
    sm.check_tool_allowed("tool1", SafetyLevel.READONLY)
    sm.check_tool_allowed("tool2", SafetyLevel.READONLY)
    logs = sm.get_recent_audit()
    assert len(logs) >= 2
    assert logs[0]["tool"] == "tool1"
    assert logs[1]["tool"] == "tool2"


def test_get_action_prefixes():
    """应有可用的 action_type 列表。"""
    prefixes = SafetyManager.get_action_prefixes()
    assert "MYCOBOT280_SAFE" in prefixes
    assert "EFINITY_PROGRAM" in prefixes
    assert "UART_LOOPBACK" in prefixes


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
    print(f"\n{'-'*40}")
    print(f"{total - failures}/{total} 通过")
    sys.exit(failures)
