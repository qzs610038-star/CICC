# final_project

本目录是分赛区决赛正式开发工程。赛方原始资料与初赛 demo 只作为只读来源，不在原目录内直接修改。

比赛任务、评分和现场流程从 [0710 最新官方细则](docs/competition_manual/第十届集创赛分赛区决赛雄芯院企业命题比赛细则_0710.md) 进入；系统架构硬边界见根目录 [AGENTS.md](../AGENTS.md)，当前完成度和阻塞见 [CURRENT_STATE.md](../CURRENT_STATE.md)。根目录 `分赛区决赛实施开发路线.md` 只作为历史路线图和经验库。

## 主线

```text
MIPI 摄像头
-> FPGA: video_in / raw_unpack / debayer / wb_gamma / roi_crop / feature_extract
-> CPU: vision_classifier / four-task matcher / per-round state / arm_controller / mycobot_protocol
-> FPGA: osd / dvi_tx
-> HDMI 显示与 myCobot 控制
```

## 状态与证据入口

- 当前完成度、阻塞、下一 Gate 和禁止项只见根目录 [CURRENT_STATE.md](../CURRENT_STATE.md)。
- 历史视频调试记录：[video_link_current_state_20260711.md](docs/debug_sessions/video_link_current_state_20260711.md) 与 [hdmi_stripe_debug_20260707.md](docs/debug_sessions/hdmi_stripe_debug_20260707.md) 仅作证据，不定义当前状态。

## 当前实施方案

- 摄像头尚未产生稳定 CSI 流期间，下游采用 host 特征回放和合成像素仿真推进，且不替代真实摄像头验收：[camera_independent_downstream_execution_plan_20260712.md](docs/technical_plans/camera_independent_downstream_execution_plan_20260712.md)。
- FPGA ROI/统计特征预处理模块的范围、接口、实施阶段和队友交接约束：[fpga_vision_preprocess_execution_plan_20260711.md](docs/technical_plans/fpga_vision_preprocess_execution_plan_20260711.md)。
- 其他已批准或待执行的技术方案统一从 [technical_plans/README.md](docs/technical_plans/README.md) 进入。

## 系统集成索引

- 动态状态：[CURRENT_STATE.md](../CURRENT_STATE.md)。
- 接口契约：`integration/`；历史架构快照：`docs/architecture/`；审查证据：`docs/review_packets/`。
- README 不维护测试计数、资源数、warning 数、构建 PASS 或板级结论，避免与证据和状态入口漂移。

## 目录边界

- `fpga/`：Efinity 工程、RTL、IP、约束和仿真。
- `cpu/`：板上 QCRV32/CPU 程序、BSP、参数和构建脚本。
- `pc_tools/`：开发期标定、串口观察和安全测试工具，不进入最终比赛闭环。
- `integration/`：寄存器、协议、引脚、视频格式等接口契约。
- `tests/`：模块仿真、CPU 单测、上板 bring-up 和验收测试。
- `docs/`：架构决策、调试记录、操作手册和证据归档。
