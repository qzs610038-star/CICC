# Agent 协作与交接优化方案评审包

本目录保存本项目多 Agent 协作、交接和状态维护机制的方案、落地记录与后续优化入口。

## 当前状态

| 能力 | 状态 | 现行入口 |
|---|---|---|
| 可变路线与当前状态 | 已落地 | 根目录 `CURRENT_STATE.md` |
| 硬边界、审查门和文档优先级 | 已落地 | 根目录 `AGENTS.md` |
| 高置信 handoff 与只读健康检查 | 已落地 | `HANDOFF_TEMPLATE.md`、`tools/agent_handoff_health_check.ps1` |
| FPGA / CPU-myCobot 模块规则 | 已落地 | `.agents/skills/fpga_vision/`、`.agents/skills/cpu_mycobot/` |
| 文档、证据、CBM 新鲜度检查 | 已落地 | `tools/project_freshness_check.ps1`、`maintenance_manifest.json` |
| 寄存器单一数据源与自动生成 | 待 SoC/APB 定版后实施 | 不得在草案阶段分配正式地址 |

## 文件索引

- [Agent协作与交接优化方案_v1.0.md](Agent协作与交接优化方案_v1.0.md)：原始问题分析、Codex 评审、落地设计和历史批注记录；其中“拟议/待审核”文本为历史提案，不应覆盖当前规则。
- [Agent协作与交接优化方案_v1.1_维护闭环.md](Agent协作与交接优化方案_v1.1_维护闭环.md)：现行维护闭环、触发条件、责任边界和未落地项。
- [maintenance_manifest.json](../../maintenance_manifest.json)：机器可检查的关键文档与刷新触发映射。
- `tools/project_freshness_check.ps1`：只读检查状态证据、受管链接、绝对路径、共享图谱和 Git 一致性。

## 使用规则

1. 合并结构性 CPU/FPGA 变更后，更新 `CURRENT_STATE.md` 和受影响的受管文档，再运行新鲜度检查。
2. 出现 handoff 时，先运行 `tools/agent_handoff_health_check.ps1`；它通过后默认接受高置信事实，仅对直接矛盾做最小范围核查。
3. 地址、时钟/复位、CDC、SoC 生成物、bitstream、机械臂动作和接线仍按 `AGENTS.md` 的 Codex Gate 处理，维护脚本不能替代工程验证。
