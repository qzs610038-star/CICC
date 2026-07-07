---
name: fpga_vision
description: Handles FPGA top-level, RTL coding, video pipelines (CSI/DSI/OSD/ROI), SDC constraints, and ModelSim simulation. Trigger when editing Verilog/RTL, constrain.sdc, mem_test.xml, .peri.xml, IP settings.json, or running Efinity/ModelSim.
---

# FPGA 视频前端与图像处理开发规范

> 全局安全红线、Codex 审查门、决赛主线边界以 `AGENTS.md`「分赛区决赛主线」和 `分赛区决赛实施开发路线.md`（未被 `CURRENT_STATE.md` 降级/覆盖的部分）为准；本 Skill 只承载 FPGA 模块细则。

当任务涉及 FPGA RTL 修改、约束调整或仿真时，必须遵守以下核心约束：

1. **仅关注 FPGA 边界**：本模块只负责视频接入、Debayer、GAMMA、ROI 提取、OSD 显示叠加与 UART/AXI 寄存器物理通道。不做颜色/形状/尺寸分类、目标匹配、阈值管理和 myCobot 协议；这些归板上 CPU（见 `cpu_mycobot` Skill）。
2. **拒绝过度设计**：不在此处编写复杂视觉识别算法或机械臂轨迹算法。
3. **时序与约束联动**：任何时钟或复位信号修改必须同步检查 `constrain.sdc` 和 `.peri.xml`；改 `mem_test.xml` 或 IP `settings.json` 触发 Codex Gate。
4. **双通道对称**：改一路逻辑时，必须同步检查通道 0 / 通道 1 是否需要一致改动（信号和模块常用 `0`/`1` 后缀区分）。
5. **编译与警告**：Efinity 编译后必须记录 Setup/Hold Slack，不可忽略 CDC Warning；warning 被判断为可忽略时触发 Codex Gate。
6. **生成产物只读**：不直接改 `ipm/`、赛方补丁、原始压缩包、波形、`outflow/`。
7. **初赛 demo 仅作经验库**：不直接迁移其中的识别 RTL、`DEMO_MODE`、临时脚本或硬编码路径。
