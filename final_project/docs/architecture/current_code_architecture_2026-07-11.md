# final_project Current Code Architecture

> 日期：2026-07-11
>
> 本文是当前代码和已归档验证证据的简明索引。真实 RTL、Efinity 工程 XML、构建日志和上板现象优先；当前路线与阻塞以根目录 `CURRENT_STATE.md` 为准。

## 系统边界

```text
MIPI camera
  -> FPGA: CSI / framebuffer / debayer / ROI / feature statistics
  -> CPU: classification / parameter table / task match / myCobot decision
  -> FPGA: result writeback / OSD / HDMI
  -> fixed-point myCobot action sequence
```

- FPGA 只提供视频前端、ROI/统计、OSD 和必要硬件通道；不恢复纯 FPGA 分类或机械臂协议主线。
- CPU 负责分类、参数管理、任务匹配和 myCobot 协议/互锁；PC `pymycobot` 只作开发期调试。

## 当前实现状态

| 区域 | 已实现 | 当前边界 |
|---|---|---|
| 视频与 HDMI | 双路 CSI/DDR/debayer/HDMI 调试链，ch1 I2C 诊断 | ch1 尚未取得稳定 parsed CSI 帧；详见 `docs/debug_sessions/video_link_current_state_20260711.md`。 |
| 预处理 | `feature_extract/`、`roi_crop/` RTL，ch1 只读旁路接入 `top.v`，map PASS | 未完成 testbench 实跑、Debugger capture、PNR、bitstream、板级帧验证、APB/CPU/OSD 接入。 |
| CPU 识别 | `board_io`、分类、参数表、匹配、主循环已在 `cpu/app/` 落地 | `soc.h`、正式 APB、RISC-V 构建和板级验证未完成。 |
| myCobot | 纯 C 协议/控制器和 mock 测试骨架存在 | 未接正式板上 UART/SoC，未确认电平/接线，未做实机动作验证。 |

## 当前系统门禁

1. 正式 CPU/APB/OSD 集成需要生成 `soc.h`、确认 APB slave/时钟复位并通过 Review Packet。
2. `TARGET_SEL`、`LIVE_FG_AREA` 和参数掉电保存均未定版；任何建议地址均不得作为硬件事实。
3. PNR 目前受 Debug Wizard `.dbg.vdb` 与未约束 I/O 问题阻塞；未生成可签核 bitstream。
4. 机械臂动作、FPGA-to-机械臂接线和实际 UART 控制必须另行走 Codex Gate。

## 入口

- 当前路线与待定决策：`../../../CURRENT_STATE.md`
- FPGA 预处理实现交接：`../technical_plans/fpga_vision_preprocess_implementation_handoff_20260711.md`
- CPU 代码/测试记录：`../../cpu/CPU_MODULE_PLAN.txt`
- SoC/APB 缺口：`generated_soc_summary_2026-07-11.md`
