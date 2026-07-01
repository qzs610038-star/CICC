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

## 目录边界

- `fpga/`：Efinity 工程、RTL、IP、约束和仿真。
- `cpu/`：板上 QCRV32/CPU 程序、BSP、参数和构建脚本。
- `pc_tools/`：开发期标定、串口观察和安全测试工具，不进入最终比赛闭环。
- `integration/`：寄存器、协议、引脚、视频格式等接口契约。
- `tests/`：模块仿真、CPU 单测、上板 bring-up 和验收测试。
- `docs/`：架构决策、调试记录、操作手册和证据归档。
