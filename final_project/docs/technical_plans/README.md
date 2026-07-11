# technical_plans

本目录保存已经批准或正在评审、尚未完全落地的技术实施方案。方案描述“准备做什么、怎样做、接口和交接条件是什么”；真实实现状态仍以 `CURRENT_STATE.md`、最新 handoff、源码、工程 XML、构建日志和上板现象为准。

## 当前方案

- [FPGA 视觉预处理模块执行与协作交接方案](fpga_vision_preprocess_execution_plan_20260711.md)：双像素 RGB888 输入、ROI、颜色/前景掩码、统计特征、帧级快照、自检仿真，以及 CPU/APB、CDC、OSD 和双通道交接约束。状态：独立 RTL/testbench 已完成，ch1 旁路已接入顶层并通过带 `mark_debug` 探针的正式 map；尚未接入 APB/CPU、OSD 或第二通道。
- [FPGA 视觉预处理模块实现交接](fpga_vision_preprocess_implementation_handoff_20260711.md)：当前 RTL 做了什么、特征和快照语义、独立综合结果、未完成项与接手者最小步骤。
- [预处理 APB/CDC 生成物门禁](../architecture/generated_soc_summary_2026-07-11.md)：确认当前缺失生成的 `soc.h`、APB 从机与 CDC RTL，并列出恢复 CPU/APB 集成所需输入。
- [预处理 APB/CDC 接口草案](../../integration/preprocess_apb_cdc_contract_draft_20260711.md)：冻结特征快照、配置提交和 CPU-to-OSD 的交接语义；不分配地址、不修改 RTL。
- [Priority 3 myCobot 板上 CPU 迁移设计](priority3_mycobot_cpu_migration_design.md)：机械臂控制从 PC 调试流程迁移到板上 CPU 的设计约束。
- [Type-C UART1 视频调试方案](typec_uart1_video_debug_plan.md)：Type-C 调试观测链路的规划和接口边界。

## 使用规则

- 开始实施前先检查根目录 `CURRENT_STATE.md` 和最新 handoff。
- 涉及 `top.v`、时钟/复位、CDC、QCRV32/APB、`mem_test.xml`、`.peri.xml`、IP `settings.json` 或 `constrain.sdc` 时，按 `AGENTS.md` 提交 Codex Review Packet。
- 不把本目录中的占位地址、旧通道命名或规划状态直接当作当前硬件事实。
- 方案完成首个可验证 checkpoint 后，应把新事实、风险和下一步同步到 `CURRENT_STATE.md` 或对应任务日志。
