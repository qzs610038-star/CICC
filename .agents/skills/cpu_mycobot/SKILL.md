---
name: cpu_mycobot
description: Handles SoC RISC-V CPU firmware, UART registers, myCobot 280 serial communication, pose interpolation, and parameters management. Trigger when editing final_project/cpu/app code, bsp.h MMIO, mycobot protocol, or running arm read-only checks.
---

# SoC CPU 与 myCobot 机械臂开发规范

> 全局安全红线、Codex 审查门、决赛主线边界以 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`（未被 `CURRENT_STATE.md` 降级/覆盖的部分）为准；本 Skill 只承载 CPU/机械臂模块细则。

当任务涉及 CPU 软件、串口控制协议或机械臂联动时，必须遵守以下核心约束：

1. **控制逻辑下放**：识别决策（颜色、尺寸分类）、目标匹配、阈值管理以及 myCobot 串口发包、重试与互锁逻辑，必须完全在板上 CPU（C 代码/固件）中实现。
2. **硬件解耦**：本模块通过 UART/寄存器接口与 FPGA 交互。不修改底层 RTL 视频流或 OSD 渲染逻辑，仅通过 AXI 寄存器回写结果（见 `fpga_vision` Skill）。
3. **机械臂安全第一（三阶段）**：
   - 第一阶段：只做环境检查、COM 口枚举、文档核对、只读状态读取方案。
   - 第二阶段：用户明确确认机械臂已固定、姿态安全、急停/断电方式明确后，才允许 RGB 灯板、读角度或极小幅动作测试。
   - 第三阶段：再讨论板上 CPU 通过 FPGA UART/GPIO 与机械臂控制器的协议桥接。未确认电平、接线和协议前，不把 FPGA 管脚直接接入机械臂控制线。
4. **波特率**：myCobot 280 串口控制固定 `1000000`；开发板 Type-C UART、JTAG-IF UART、UART2 波特率须按对应固件/RTL 另行确认。
5. **PC 仅开发期**：`pymycobot`、myBlockly、PC 只用于调试/标定/日志，不进正式识别控制闭环（见 `mycobot_pc_tests/` 归档说明）。
6. **动作类修改触发 Codex Gate**：涉及实际动作、夹爪、快速移动、FPGA-to-机械臂接线、CP210x 驱动安装或 `pymycobot` 控制脚本时，必须生成 Codex Review Packet。
