# docs

本目录放人要读的设计思路、决策理由、调试经验、操作说明、迁移日志和证据。

寄存器地址、协议帧、引脚号、视频格式等机器要遵守的接口契约放在 `integration/`。

`source_materials_digest/` 放赛方原始资料包的 Markdown 摘要入口，供没有原件的队友和本地 agent 快速上手；原件线下获取，仓库不收录。

## 目录索引

- `architecture/`：架构设计、关键决策和当前代码链路说明。
- 视频像素格式、时钟域、ROI/统计快照与 OSD 帧对齐学习讲义：[视频流水线与时序学习讲义](architecture/video_pipeline_timing_learning_guide_20260718.md)。
- 当前活跃路线、阻塞和待定决策：根目录 [CURRENT_STATE.md](../../CURRENT_STATE.md)。
- 当前综合架构快照：[current_code_architecture_2026-07-13.md](architecture/current_code_architecture_2026-07-13.md)；[2026-07-11 快照](architecture/current_code_architecture_2026-07-11.md)及更早文档仅作历史证据。
- `technical_plans/`：已批准或待审核的实施方案。摄像头未恢复期间的并行路线见 [摄像头不可用期间的下游并行推进方案](technical_plans/camera_independent_downstream_execution_plan_20260712.md)，当前 FPGA 预处理入口见 [FPGA 视觉预处理模块执行与协作交接方案](technical_plans/fpga_vision_preprocess_execution_plan_20260711.md)。
- `debug_sessions/`：当前调试状态和逐轮上板记录。
- `review_packets/`：跨子系统、顶层、时钟/CDC、工程配置等 Codex 审查材料；当前团队整合入口见 [2026-07-13 团队整合 Review Packet](review_packets/team_integration_merge_review_20260713.md)。
- `bringup/`：板级 bring-up 操作说明。
- `competition_manual/`：官方比赛细则与现场操作手册。核心约束入口见 [0710 最新官方细则](competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md)，项目差距与优化顺序见 [细则对照优化建议](competition_manual/细则对照项目优化建议_20260712.md)。
- `evidence/`：测试和验收证据。
- `source_materials_digest/`：赛方资料摘要入口。
