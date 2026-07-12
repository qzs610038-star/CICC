---
name: fpga_vision
description: Handles FPGA top-level, RTL coding, video pipelines (CSI/DSI/OSD/ROI), SDC constraints, and ModelSim simulation. Trigger when editing Verilog/RTL, constrain.sdc, mem_test.xml, .peri.xml, IP settings.json, or running Efinity/ModelSim.
---

# FPGA 视频前端与图像处理开发规范

> 比赛任务以最新官方细则为准，系统职责以 `AGENTS.md`「分赛区决赛系统架构硬边界」为准，当前进度与阻塞以 `CURRENT_STATE.md` 为准；`分赛区决赛实施开发路线.md` 只作历史经验。本 Skill 只承载 FPGA 模块细则。

当任务涉及 FPGA RTL 修改、约束调整或仿真时，必须遵守以下核心约束：

1. **仅关注 FPGA 边界**：本模块只负责视频接入、Debayer、GAMMA、ROI 提取、基础统计特征、OSD 像素渲染与 UART/AXI/FIFO 物理通道。OSD 的任务/判断/动作语义由板上 CPU 产生；FPGA 不做颜色/形状/尺寸分类、四任务关系判定、逐轮事务、阈值管理或 myCobot 协议（见 `cpu_mycobot` Skill）。
2. **拒绝过度设计**：不在此处编写复杂视觉识别算法或机械臂轨迹算法。
3. **时序与约束联动**：任何时钟或复位信号修改必须同步检查 `constrain.sdc` 和 `.peri.xml`；改 `mem_test.xml` 或 IP `settings.json` 触发 Codex Gate。
4. **双通道对称**：改一路逻辑时，必须同步检查通道 0 / 通道 1 是否需要一致改动（信号和模块常用 `0`/`1` 后缀区分）。
5. **编译与警告**：Efinity 编译后必须记录 Setup/Hold Slack，不可忽略 CDC Warning；warning 被判断为可忽略时触发 Codex Gate。
6. **生成产物只读**：不直接改 `ipm/`、赛方补丁、原始压缩包、波形、`outflow/`。
7. **初赛 demo 仅作经验库**：不直接迁移其中的识别 RTL、`DEMO_MODE`、临时脚本或硬编码路径。
8. **目标不等于状态**：架构允许某模块存在，不代表该模块已接入、已综合、已烧录或已上板；完成度必须回查 `CURRENT_STATE.md` 和真实日志。
