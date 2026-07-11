# docs

本目录放人要读的设计思路、决策理由、调试经验、操作说明、迁移日志和证据。

寄存器地址、协议帧、引脚号、视频格式等机器要遵守的接口契约放在 `integration/`。

`source_materials_digest/` 放赛方原始资料包的 Markdown 摘要入口，供没有原件的队友和本地 agent 快速上手；原件线下获取，仓库不收录。

## 目录索引

- `architecture/`：架构设计、关键决策和当前代码链路说明。
- 当前活跃路线、阻塞和待定决策：根目录 [CURRENT_STATE.md](../../CURRENT_STATE.md)。
- 当前综合架构快照：[current_code_architecture_2026-07-11.md](architecture/current_code_architecture_2026-07-11.md)；旧日期架构文档仅作历史证据。
- `technical_plans/`：已批准或待审核的实施方案。当前 FPGA 预处理入口见 [FPGA 视觉预处理模块执行与协作交接方案](technical_plans/fpga_vision_preprocess_execution_plan_20260711.md)。
- `debug_sessions/`：当前调试状态和逐轮上板记录。
- `review_packets/`：跨子系统、顶层、时钟/CDC、工程配置等 Codex 审查材料。
- `bringup/`：板级 bring-up 操作说明。
- `competition_manual/`：比赛现场操作手册。
- `evidence/`：测试和验收证据。
- `source_materials_digest/`：赛方资料摘要入口。
