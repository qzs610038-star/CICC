"""
fpga_robot_mcp.review_packet — Review Packet 模板生成。

实现:
- generate_review_packet(context) — 根据当前环境状态生成 Review Packet Markdown
- _format_hardware_state() — 格式化硬件状态章节
- _format_risks() — 格式化风险列表
- _format_questions() — 格式化问题列表
"""

from __future__ import annotations

import datetime
from typing import Any


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
    """
    task = context.get("task", "（未指定）")
    action_type = context.get("action_type", "（未指定）")
    config = context.get("config", {})
    risks = context.get("risks", [])
    questions = context.get("questions", [])

    # 从 config 提取路径
    project_root = _get_nested(config, "project_root", "（未知）")
    efinity = config.get("efinity", {})
    efinity_home = efinity.get("home", "（未知）")
    efinity_bin = efinity.get("bin", "（未知）")
    project_xml = efinity.get("project_xml", "（未知）")

    # safety 配置摘要
    safety_cfg = config.get("safety", {})
    safety_parts = []
    for k in ["allow_hardware_actions", "require_confirm_token"]:
        v = safety_cfg.get(k)
        if v is not None:
            safety_parts.append(f"{k}={v}")
    safety_config = ", ".join(safety_parts) if safety_parts else "（默认）"

    # 各部分
    hardware_state = _format_hardware_state(context)
    risks_md = _format_risks(risks)
    questions_md = _format_questions(questions)

    # 标题
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    title = f"Review Packet — {task[:48]}{'…' if len(task) > 48 else ''} ({now})"

    markdown = _TEMPLATE.format(
        title=title,
        task=task,
        action_type=action_type,
        project_root=project_root,
        efinity_home=efinity_home,
        efinity_bin=efinity_bin,
        project_xml=project_xml,
        safety_config=safety_config,
        hardware_state=hardware_state,
        risks=risks_md,
        questions=questions_md,
    )

    word_count = len(markdown.split())

    return {
        "status": "ok",
        "data": {
            "markdown": markdown,
            "word_count": word_count,
            "title": title.split("—")[0].strip(),
            "generated_at": now,
        },
    }


def _format_hardware_state(context: dict) -> str:
    """
    格式化硬件状态章节。
    尝试从 context 中提取串口、JTAG、myCobot 状态信息。
    """
    lines = []
    safety_entries = context.get("safety", [])

    # 提取最近的硬件相关工具调用
    hw_calls = [e for e in safety_entries if e.get("level") in ("hardware_side_effect", "write_file")]
    if hw_calls:
        lines.append("**最近硬件操作记录**:")
        for e in hw_calls[-5:]:
            icon = "✅" if e.get("approved") else "❌"
            lines.append(f"- {icon} {e.get('tool', '?')} | {e.get('detail', '')}")
        lines.append("")
    else:
        lines.append("**最近硬件操作**:（无记录）")
        lines.append("")

    # myCobot 状态
    lines.append("**串口状态**: 请运行 `board_list_uart_candidates` 或 `mycobot280_list_ports` 获取")
    lines.append("")
    lines.append("**JTAG 状态**: 请运行 `board_check_jtag_chain` 获取")
    lines.append("")
    lines.append("**myCobot 280 状态**: 请运行 `mycobot280_check_env` 获取")

    return "\n".join(lines)


def _format_risks(risks: list[str]) -> str:
    """格式化风险列表为 Markdown 列表。"""
    if not risks:
        return "（无）"
    lines = []
    for i, risk in enumerate(risks, 1):
        lines.append(f"{i}. {risk}")
    return "\n".join(lines)


def _format_questions(questions: list[str]) -> str:
    """格式化问题列表为 Markdown 列表。"""
    if not questions:
        return "（无）"
    lines = []
    for i, q in enumerate(questions, 1):
        lines.append(f"{i}. {q}")
    return "\n".join(lines)


def _get_nested(d: dict, key: str, default: Any = None) -> Any:
    """安全获取嵌套字典的值。"""
    keys = key.split(".")
    for k in keys:
        if isinstance(d, dict):
            d = d.get(k, {})
        else:
            return default
    return d if d != {} else default
