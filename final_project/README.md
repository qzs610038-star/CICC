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

## 当前视频链路状态

- 当前双摄像头视频链路、上板证据、LED 诊断映射、SW4 约束与下一步分支：[video_link_current_state_20260711.md](docs/debug_sessions/video_link_current_state_20260711.md)。
- 每轮 HDMI/MIPI 上板调试的详细历史日志：[hdmi_stripe_debug_20260707.md](docs/debug_sessions/hdmi_stripe_debug_20260707.md)。
- 当前硬件状态：ch1 I2C 控制有响应，但尚未观测到稳定 CSI 视频帧；请优先阅读上述现状摘要，不要根据早期日志重复修改 HDMI/DDR 下游逻辑。

## 当前实施方案

- 摄像头尚未产生稳定 CSI 流期间，下游采用 host 特征回放和合成像素仿真推进，且不替代真实摄像头验收：[camera_independent_downstream_execution_plan_20260712.md](docs/technical_plans/camera_independent_downstream_execution_plan_20260712.md)。
- FPGA ROI/统计特征预处理模块的范围、接口、实施阶段和队友交接约束：[fpga_vision_preprocess_execution_plan_20260711.md](docs/technical_plans/fpga_vision_preprocess_execution_plan_20260711.md)。
- 其他已批准或待执行的技术方案统一从 [technical_plans/README.md](docs/technical_plans/README.md) 进入。

## 当前系统集成状态

- 两位队友分支与个人工作区已在本地整合：Host CPU/competition 回归 795/795 PASS，8 个关键 C 源完成 Efinity RISC-V strict compile-only；正式链接、烧录和板级运行尚未验证。详见 [团队整合 Review Packet](docs/review_packets/team_integration_merge_review_20260713.md)。
- CPU 侧已有五色/四任务 matcher、完整 `round_controller` 和轻量 `competition_round_transaction`；`main.c` 尚未接入两层逐轮控制器、正式 APB/OSD 或 arm done/ACK。
- FPGA 合成像素源已接入当前验证路径，整合工程 map PASS；PNR 因 1,776 个 IO 无 placement 与 `outpad` 断言 FAIL，没有可验收 bitstream。合成源不替代真实摄像头。
- myCobot 的板上 CPU 协议与控制器已有代码和 mock 测试骨架；正式 UART、接线/电平和动作验证尚未开始，PC `pymycobot` 不进入比赛闭环，真实动作维持 NO-GO。
- CPU/APB/OSD 仍受生成 SoC 产物门禁：当前缺少正式 `soc.h`、APB slave、CDC RTL 和地址分配，见 [生成 SoC 摘要](docs/architecture/generated_soc_summary_2026-07-11.md)。
- 全项目的当前路线、阻塞和待定决策以根目录 [CURRENT_STATE.md](../CURRENT_STATE.md) 为准。
- 当前代码分层与门禁见 [2026-07-13 综合架构快照](docs/architecture/current_code_architecture_2026-07-13.md)。

## 目录边界

- `fpga/`：Efinity 工程、RTL、IP、约束和仿真。
- `cpu/`：板上 QCRV32/CPU 程序、BSP、参数和构建脚本。
- `pc_tools/`：开发期标定、串口观察和安全测试工具，不进入最终比赛闭环。
- `integration/`：寄存器、协议、引脚、视频格式等接口契约。
- `tests/`：模块仿真、CPU 单测、上板 bring-up 和验收测试。
- `docs/`：架构决策、调试记录、操作手册和证据归档。
