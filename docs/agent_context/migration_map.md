# G3 约束迁移映射

| 分类 | 旧位置 | 新位置 | 保留语义 | 验证方法 |
|---|---|---|---|---|
| 稳定硬约束 | AGENTS 架构、官方规则、安全与审查门 | AGENTS 稳定章节 | FPGA/CPU/PC 分工、比赛约束、机械臂、Codex/handoff/merge 门 | required markers + 等价性问答 |
| 当前动态事实 | AGENTS/CLAUDE/Skills/CURRENT_STATE 重复 | CURRENT_STATE | 固定 SHA、NOT VERIFIED、下一 Gate、禁用动作 | freshness + 状态字段检查 |
| 历史证据 | CURRENT_STATE 原 1–566 行 | state_history 原文归档 | 日期、措辞、证据、替代、失效条件 | SHA-256 + 566 行一一映射 |
| 操作 runbook | AGENTS/CLAUDE 常用命令 | docs/agent_context/operations_runbook.md | 只读检查、构建/Hello 边界 | Markdown link + marker |
| Review Packet 模板 | AGENTS/CLAUDE 重复模板 | docs/agent_context/review_packet_template.md | 可复现命令、warning、风险与安全状态 | link check |
| CBM 动态信息 | AGENTS/CLAUDE 节点数/commit | CBM_CONFIG_GUIDE + artifact.json | 图谱只定位，真实文件最终裁定 | 禁止动态数字扫描 |
| 重复副本 | 两个 Skill 的当前 SHA/构建描述 | CURRENT_STATE 路由 | 模块红线保留，动态状态单一真源 | skill marker 检查 |
| 已被覆盖状态 | CURRENT_STATE 历史 PASS/hash/计数 | state_history | 不丢失但不参与当前判定 | archive hash + freshness |

任何未映射内容不得删除；新增迁移必须先更新本表。
