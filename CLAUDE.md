# Claude 仓库适配层

> 本文件只定义 Claude 专属路由、执行方式与交接。稳定规则见 [AGENTS.md](AGENTS.md)，动态状态见 [CURRENT_STATE.md](CURRENT_STATE.md)。不得在此复制 SHA、构建批次、hash、节点/资源/warning/测试计数或板级结论。

## ENTRY_ROUTING

Claude 开始任务时依次读取：

1. [AGENTS.md](AGENTS.md)：稳定架构、安全、审查门和优先级。
2. [0710 比赛细则](final_project/docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md)：官方任务与现场约束。
3. [CURRENT_STATE.md](CURRENT_STATE.md)：当前快照、阻塞、下一 Gate 和禁止项。
4. [决赛主方案](final_project/docs/technical_plans/competition_score_maximization_execution_plan_20260712.md)：执行顺序与回退。
5. 本文件：Claude 专属行为。

若入口冲突，按 `AGENTS.md` 的文档优先级处理；不得从历史计划、旧 handoff、Host 测试或架构目标推断板级闭环。

## CLAUDE_EXECUTION_MODE

### 架构初设

- 用于跨模块、视频/AXI/framebuffer、时钟复位或顶层策略；默认只读探索。
- 分别列出官方约束、AGENTS 稳定边界、CURRENT_STATE 当前事实，再核对真实源码/XML/日志。
- 输出目标、涉及文件、信号链、时钟/复位、双通道影响、备选方案、风险与验证计划；形成 Review Packet 后等待 Codex Gate。

### 具体执行

- 每轮只处理一个明确问题，保持小 diff，不重排无关生成文件。
- 修改前说明影响范围及是否改变四任务、逐轮事务、OSD 语义或机械臂唯一响应。
- 修改后列出文件、命令、退出码、日志、warning 与未验证项。
- 连续两轮同错未解决时停止补丁，生成 Codex Review Packet。

### 入口发现

- 优先用 codebase-memory 图谱定位，再回到真实文件核查；图谱无结果不等于源码缺失。
- 模块操作命令统一见 [Agent runbook](docs/agent_context/operations_runbook.md)，不要在本文件维护易漂移命令和状态。

## CLAUDE_HANDOFF

- 修改前运行 `tools/agent_handoff_health_check.ps1`；触发矛盾条件时输出 `[Contradiction Report]`。
- 阶段结束使用 `.claude/commands/fpga-handoff.md` 的 JSON 模板；只记录恢复所需事实、相对路径、命令结果、未验证项和下一 Gate，不写密钥或长日志。
- 交给 Codex 的包使用 [review_packet_template.md](docs/agent_context/review_packet_template.md)，并明确机械臂/外设是否涉及、是否执行动作及安全确认。

## REQUIRED_BOUNDARY_POINTERS

- FPGA/CPU/PC 职责：`AGENTS.md`「分赛区决赛系统架构硬边界」。
- 当前单摄 Gate、I0 UART1/myCobot UART2 波特率和禁止项：`CURRENT_STATE.md`。
- 冻结接口与 wsc/libaoxun/qzs 文件范围：`docs/agent_context/TEAM_INTERFACE_FREEZE_AND_FINAL_DAY_OWNERSHIP_20260719.md`。
- 合并治理：`docs/merge_governance/`。
- 模块细则：`.agents/skills/fpga_vision/SKILL.md`、`.agents/skills/cpu_mycobot/SKILL.md`。
