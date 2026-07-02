"""
fpga_robot_mcp.review_packet — Review Packet 模板生成。

⬅️ 留待经济模型填充。

需要实现的函数:
- generate_review_packet(context) — 根据当前环境状态生成 Review Packet Markdown
"""

from __future__ import annotations

from typing import Any


# ── Review Packet 模板 ──────────────────────────────────────

_TEMPLATE = """# {title}

## 任务目标

{task}

## 本轮动作类型

- {action_type}

## 项目与工具状态

- 项目根目录：{project_root}
- Efinity 路径：{efinity_home}
- CLI 路径：{efinity_bin}
- 主工程 XML：{project_xml}
- safety 配置：{safety_config}

## 硬件状态

{hardware_state}

## 风险与未验证项

{risks}

## 希望 Codex 判断的问题

{questions}
"""


def generate_review_packet(context: dict) -> dict:
    """
    根据当前环境状态生成 Review Packet Markdown。

    参数 context 包含:
        task: str — 任务目标
        action_type: str — 动作类型
        config: dict — 当前配置（已隐藏敏感字段）
        safety: list[dict] — 最近的审计记录
        risks: list[str] — 风险项
        questions: list[str] — 希望 Codex 判断的问题

    实现说明:
    1. 从 config 中提取项目路径、Efinity 路径等信息
    2. 从 context 中提取风险、问题等
    3. 填充 _TEMPLATE 模板
    4. 如果需要，写入证据目录（可选）

    返回格式:
    {
        "status": "ok",
        "markdown": "...",  # 完整 Review Packet Markdown
        "word_count": 500,
    }
    """
    # TODO(cheap-model): 填充模板生成 Review Packet
    # 参考 v0.2 §Review Packet 模板 中的格式
    raise NotImplementedError("review_packet.generate_review_packet() — 需填充 Review Packet 模板")


# ══════════════════════════════════════════════════════════════
# ⬅️ 以下函数可进一步拆分（经济模型按需实现）
# ══════════════════════════════════════════════════════════════


def _format_hardware_state(context: dict) -> str:
    """
    格式化硬件状态章节。
    """
    # TODO(cheap-model): 提取串口、JTAG、myCobot 状态
    raise NotImplementedError("review_packet._format_hardware_state() — 需格式化硬件状态")


def _format_risks(risks: list[str]) -> str:
    """
    格式化风险列表为 Markdown 列表。
    """
    # TODO(cheap-model): 将风险列表格式化为 Markdown
    raise NotImplementedError("review_packet._format_risks() — 需格式化风险列表")


def _format_questions(questions: list[str]) -> str:
    """
    格式化问题列表为 Markdown 列表。
    """
    # TODO(cheap-model): 将问题列表格式化为 Markdown
    raise NotImplementedError("review_packet._format_questions() — 需格式化问题列表")
