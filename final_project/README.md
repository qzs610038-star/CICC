# final_project

本目录是分赛区决赛正式开发工程。赛方原始资料与初赛 demo 只作为只读来源，不在原目录内直接修改。

## 主线

```text
MIPI 摄像头
-> FPGA: video_in / raw_unpack / debayer / wb_gamma / roi_crop / feature_extract
-> CPU: vision_classifier / task_matcher / arm_controller / mycobot_protocol
-> FPGA: osd / dvi_tx
-> HDMI 显示与 myCobot 控制
```

## 当前视频链路状态

- 当前双摄像头视频链路、上板证据、LED 诊断映射、SW4 约束与下一步分支：[video_link_current_state_20260711.md](docs/debug_sessions/video_link_current_state_20260711.md)。
- 每轮 HDMI/MIPI 上板调试的详细历史日志：[hdmi_stripe_debug_20260707.md](docs/debug_sessions/hdmi_stripe_debug_20260707.md)。
- 当前硬件状态：ch1 I2C 控制有响应，但尚未观测到稳定 CSI 视频帧；请优先阅读上述现状摘要，不要根据早期日志重复修改 HDMI/DDR 下游逻辑。

## 当前实施方案

- FPGA ROI/统计特征预处理模块的范围、接口、实施阶段和队友交接约束：[fpga_vision_preprocess_execution_plan_20260711.md](docs/technical_plans/fpga_vision_preprocess_execution_plan_20260711.md)。
- 其他已批准或待执行的技术方案统一从 [technical_plans/README.md](docs/technical_plans/README.md) 进入。

## 当前系统集成状态

- CPU 识别侧的 `board_io`、分类、参数表、任务匹配和主循环代码已落地；当前单测结果以 [CPU_MODULE_PLAN.txt](cpu/CPU_MODULE_PLAN.txt) 的“最新归档”统计为准，尚未完成 RISC-V/板级验证。
- myCobot 的板上 CPU 协议与控制器已有代码和 mock 测试骨架；正式 UART、接线/电平和动作验证尚未开始，PC `pymycobot` 不进入比赛闭环。
- CPU/APB/OSD 仍受生成 SoC 产物门禁：当前缺少正式 `soc.h`、APB slave、CDC RTL 和地址分配，见 [生成 SoC 摘要](docs/architecture/generated_soc_summary_2026-07-11.md)。
- 全项目的当前路线、阻塞和待定决策以根目录 [CURRENT_STATE.md](../CURRENT_STATE.md) 为准。

## 目录边界

- `fpga/`：Efinity 工程、RTL、IP、约束和仿真。
- `cpu/`：板上 QCRV32/CPU 程序、BSP、参数和构建脚本。
- `pc_tools/`：开发期标定、串口观察和安全测试工具，不进入最终比赛闭环。
- `integration/`：寄存器、协议、引脚、视频格式等接口契约。
- `tests/`：模块仿真、CPU 单测、上板 bring-up 和验收测试。
- `docs/`：架构决策、调试记录、操作手册和证据归档。
