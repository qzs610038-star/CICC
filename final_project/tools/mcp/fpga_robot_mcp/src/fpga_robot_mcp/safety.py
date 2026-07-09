"""
fpga_robot_mcp.safety — 三级安全门控、confirm token、审计日志。

工具分类：
- READONLY: 读文件/枚举/状态检查，默认允许
- WRITE_FILE: 写文件/构建产物，需 allow_hardware_actions 或 dry_run
- HARDWARE_SIDE_EFFECT: 烧录/机械臂动作，需 allow_hardware_actions + confirm_token
"""

from __future__ import annotations

import datetime
import json
import os
import sys
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Optional

from fpga_robot_mcp.config import FpgaRobotConfig


class SafetyLevel(Enum):
    """安全等级枚举。"""

    READONLY = "readonly"
    """只读操作：路径检查、串口枚举、状态查询。默认允许。"""

    WRITE_FILE = "write_file"
    """写文件/构建产物：需要 allow_hardware_actions=true 或 dry_run=true。"""

    HARDWARE_SIDE_EFFECT = "hardware_side_effect"
    """硬件副作用：烧录 bitstream、机械臂动作、夹爪控制。
       需要 allow_hardware_actions=true + confirm_token 匹配。"""


@dataclass
class AuditEntry:
    """单条审计记录。"""

    tool: str
    params: dict
    safety_level: SafetyLevel
    approved: bool
    timestamp: str = ""
    message: str = ""
    detail: str = ""

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")


# ── 不同动作类型的 confirm token 前缀 ───────────────────────

CONFIRM_PREFIXES = {
    "MYCOBOT280_SAFE": "myCobot 280 安全确认",
    "EFINITY_PROGRAM": "Efinity 烧录确认",
    "UART_LOOPBACK": "UART 回环测试确认",
}

# tool_name → 应匹配的 action_type
TOOL_ACTION_MAP: dict[str, str] = {
    "efinity_program_bitstream": "EFINITY_PROGRAM",
    "mycobot280_set_rgb": "MYCOBOT280_SAFE",
    "mycobot280_execute_motion": "MYCOBOT280_SAFE",
    "mycobot280_control_gripper": "MYCOBOT280_SAFE",
    "board_uart_loopback_test": "UART_LOOPBACK",
}


def _redacted_token_detail(confirm_token: Optional[str]) -> str:
    """Return an audit-safe token marker without storing the secret value."""
    if not confirm_token:
        return "confirm_token=missing"
    return "confirm_token=present_redacted"


class SafetyManager:
    """安全门控管理器。

    用法:
        sm = SafetyManager(config)
        ok, msg = sm.check_tool_allowed("efinity_program_bitstream",
                                         SafetyLevel.HARDWARE_SIDE_EFFECT,
                                         confirm_token="I_CONFIRM_EFINITY_PROGRAM_20260702")
    """

    def __init__(self, config: FpgaRobotConfig):
        self._config = config
        self.allow_hardware = config.safety.allow_hardware_actions
        self.require_confirm = config.safety.require_confirm_token
        self.audit_log: list[AuditEntry] = []

    # ── confirm token ───────────────────────────────────────

    @staticmethod
    def generate_confirm_token(action_type: str) -> str:
        """生成确认 token，格式: I_CONFIRM_{action_type}_YYYYMMDD

        >>> SafetyManager.generate_confirm_token("MYCOBOT280_SAFE")
        'I_CONFIRM_MYCOBOT280_SAFE_20260702'
        """
        date_str = datetime.date.today().strftime("%Y%m%d")
        return f"I_CONFIRM_{action_type}_{date_str}"

    @staticmethod
    def validate_confirm_token(token: str, action_type: str) -> bool:
        """校验 token 格式是否匹配：前缀 + 8 位数字日期。"""
        expected_prefix = f"I_CONFIRM_{action_type}_"
        if not token.startswith(expected_prefix):
            return False
        date_part = token[len(expected_prefix):]
        if len(date_part) != 8:
            return False
        return date_part.isdigit()

    @staticmethod
    def get_action_prefixes() -> dict[str, str]:
        """返回所有可用的 action_type → 描述。"""
        return dict(CONFIRM_PREFIXES)

    # ── 门控检查 ───────────────────────────────────────────

    def check_tool_allowed(
        self,
        tool_name: str,
        level: SafetyLevel,
        confirm_token: Optional[str] = None,
        dry_run: bool = False,
    ) -> tuple[bool, str]:
        """
        检查工具是否允许执行。

        参数:
            tool_name: 工具名（用于审计日志）
            level: 安全等级
            confirm_token: 硬件副作用操作所需的确认 token
            dry_run: 写文件操作可设为 True 进行 dry-run

        返回:
            (allowed: bool, message: str)
        """
        # ── READONLY ──
        if level == SafetyLevel.READONLY:
            self._log_audit(tool_name, level, True)
            return True, "只读操作，允许执行"

        # ── WRITE_FILE ──
        if level == SafetyLevel.WRITE_FILE:
            if dry_run:
                self._log_audit(tool_name, level, True, detail="dry_run")
                return True, "dry-run 模式，允许执行"
            if self.allow_hardware:
                self._log_audit(tool_name, level, True)
                return True, "硬件动作已允许（allow_hardware_actions=true），可执行写文件操作"
            self._log_audit(tool_name, level, False, detail="allow_hardware_actions=false")
            return False, (
                "写文件操作被拒绝：allow_hardware_actions=false。"
                "请设置 allow_hardware_actions=true 或使用 dry_run=true"
            )

        # ── HARDWARE_SIDE_EFFECT ──
        if level == SafetyLevel.HARDWARE_SIDE_EFFECT:
            if dry_run:
                self._log_audit(tool_name, level, True, detail="dry_run")
                return True, "dry-run 模式，不执行硬件副作用，允许生成执行计划"
            if not self.allow_hardware:
                self._log_audit(tool_name, level, False, detail="allow_hardware_actions=false")
                return False, (
                    "硬件副作用操作被拒绝：allow_hardware_actions=false。"
                    "请在配置中设置 allow_hardware_actions=true 并重启服务"
                )
            if self.require_confirm and not confirm_token:
                self._log_audit(tool_name, level, False, detail="缺少 confirm_token")
                return False, (
                    "硬件副作用操作需要 confirm_token。"
                    f"请提供有效的确认 token（格式：I_CONFIRM_<ACTION>_YYYYMMDD）"
                )
            if self.require_confirm and confirm_token:
                # 根据工具名确定应匹配的 action_type
                expected_action = TOOL_ACTION_MAP.get(tool_name)
                if expected_action:
                    if not self.validate_confirm_token(confirm_token, expected_action):
                        self._log_audit(tool_name, level, False, detail=f"confirm_token 与 {tool_name} 不匹配，应使用 {expected_action}")
                        return False, (
                            f"confirm_token 与工具 {tool_name} 不匹配。"
                            f"请使用对应 {expected_action} 类型的确认 token"
                        )
                else:
                    # 回退：尝试匹配所有已知 action_type
                    matched = False
                    for action_type in CONFIRM_PREFIXES:
                        if self.validate_confirm_token(confirm_token, action_type):
                            matched = True
                            break
                    if not matched:
                        self._log_audit(tool_name, level, False, detail="confirm_token 格式不匹配")
                        return False, (
                            f"confirm_token '{confirm_token}' 格式不匹配。"
                            f"支持的 action_type: {', '.join(CONFIRM_PREFIXES.keys())}"
                        )
            self._log_audit(tool_name, level, True, detail=_redacted_token_detail(confirm_token))
            return True, "硬件副作用操作已确认，允许执行"

        self._log_audit(tool_name, level, False, detail="未知安全等级")
        return False, f"未知安全等级: {level}"

    def check_and_return(
        self,
        tool_name: str,
        level: SafetyLevel,
        confirm_token: Optional[str] = None,
        dry_run: bool = False,
    ) -> dict:
        """check_tool_allowed 的字典返回版本，方便 MCP 工具直接返回。"""
        allowed, msg = self.check_tool_allowed(tool_name, level, confirm_token, dry_run)
        return {
            "safety_allowed": allowed,
            "safety_message": msg,
        }

    # ── 审计日志 ───────────────────────────────────────────

    def _log_audit(
        self,
        tool: str,
        level: SafetyLevel,
        approved: bool,
        detail: str = "",
    ):
        entry = AuditEntry(
            tool=tool,
            params={},
            safety_level=level,
            approved=approved,
            detail=detail,
        )
        self.audit_log.append(entry)
        self._write_audit_entry(entry)

    def _write_audit_entry(self, entry: AuditEntry):
        """将审计条目写入 stderr（MCP 协议 stderr 流）。"""
        print(
            f"[audit] {entry.timestamp} | {entry.tool} "
            f"| {entry.safety_level.value} | {'✅' if entry.approved else '❌'} "
            f"| {entry.detail}",
            file=sys.stderr,
        )

    def get_recent_audit(self, limit: int = 20) -> list[dict]:
        """返回最近的审计记录。"""
        entries = self.audit_log[-limit:]
        return [
            {
                "timestamp": e.timestamp,
                "tool": e.tool,
                "level": e.safety_level.value,
                "approved": e.approved,
                "detail": e.detail,
            }
            for e in entries
        ]


# ── 快速测试 ────────────────────────────────────────────────

if __name__ == "__main__":
    from fpga_robot_mcp.config import get_default_config

    cfg = get_default_config()
    sm = SafetyManager(cfg)

    print("=== 安全门控测试 ===")

    # READONLY
    ok, msg = sm.check_tool_allowed("fpga_robot_status", SafetyLevel.READONLY)
    print(f"  READONLY: {ok} — {msg}")

    # WRITE_FILE dry_run
    ok, msg = sm.check_tool_allowed("efinity_run_build", SafetyLevel.WRITE_FILE, dry_run=True)
    print(f"  WRITE_FILE(dry_run): {ok} — {msg}")

    # WRITE_FILE no permission
    ok, msg = sm.check_tool_allowed("efinity_run_build", SafetyLevel.WRITE_FILE)
    print(f"  WRITE_FILE(denied): {ok} — {msg}")

    # HARDWARE no token
    ok, msg = sm.check_tool_allowed("efinity_program_bitstream", SafetyLevel.HARDWARE_SIDE_EFFECT)
    print(f"  HARDWARE(no token): {ok} — {msg}")

    # HARDWARE bad token
    ok, msg = sm.check_tool_allowed(
        "efinity_program_bitstream", SafetyLevel.HARDWARE_SIDE_EFFECT,
        confirm_token="BAD_TOKEN"
    )
    print(f"  HARDWARE(bad token): {ok} — {msg}")

    # Generate token
    token = sm.generate_confirm_token("MYCOBOT280_SAFE")
    print(f"  生成 token: {token}")

    # Allow hardware first
    cfg.safety.allow_hardware_actions = True
    sm.allow_hardware = True
    ok, msg = sm.check_tool_allowed(
        "mycobot280_execute_motion", SafetyLevel.HARDWARE_SIDE_EFFECT,
        confirm_token=token
    )
    print(f"  HARDWARE(with token): {ok} — {msg}")
