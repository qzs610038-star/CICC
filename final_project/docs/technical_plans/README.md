# technical_plans

本目录保存已经批准或正在评审、尚未完全落地的技术实施方案。方案描述“准备做什么、怎样做、接口和交接条件是什么”；真实实现状态仍以 `CURRENT_STATE.md`、最新 handoff、源码、工程 XML、构建日志和上板现象为准。

## 当前方案

- **[决赛主方案：最大化得分总控执行方案](competition_score_maximization_execution_plan_20260712.md)**：当前最高执行方案；版本、已完成项和下一 Gate 以文件正文及根目录 `CURRENT_STATE.md` 为准，不在索引复制。
- [三成员决赛保底冲刺执行板](three_member_execution_board_20260712_17.md)：把主方案拆成A（FPGA/SoC）、B（CPU负责人）、C（机械臂/现场）三张逐日任务卡，包含文件所有权、当日验收、交接格式和失败降级规则。
- [摄像头不可用期间的下游并行推进方案](camera_independent_downstream_execution_plan_20260712.md)：状态：部分实施。Host 回放、合成像素源、竞赛契约和逐轮事务已形成证据；正式 SoC/APB/CDC/OSD、PNR 和板级闭环未完成，仍是非正式比赛路径。
- [FPGA/SoC 分阶段执行计划](a_fpga_soc_execution_plan_20260712.md)：A2–A4 已有隔离工程和 Review Packet 证据；正式协作工程继续受 Codex Gate，当前优先关闭 production/debug 与 periphery/PNR 门。
- [FPGA 视觉预处理模块执行与协作交接方案](fpga_vision_preprocess_execution_plan_20260711.md)：双像素 RGB888 输入、ROI、颜色/前景掩码、统计特征、帧级快照、自检仿真，以及 CPU/APB、CDC、OSD 和双通道交接约束。状态：独立 RTL/testbench 已完成，ch1 旁路已接入顶层并通过带 `mark_debug` 探针的正式 map；尚未接入 APB/CPU、OSD 或第二通道。
- [FPGA 视觉预处理模块实现交接](fpga_vision_preprocess_implementation_handoff_20260711.md)：当前 RTL 做了什么、特征和快照语义、独立综合结果、未完成项与接手者最小步骤。
- [预处理 APB/CDC 生成物门禁](../architecture/generated_soc_summary_2026-07-11.md)：确认当前缺失生成的 `soc.h`、APB 从机与 CDC RTL，并列出恢复 CPU/APB 集成所需输入。
- [预处理 APB/CDC 接口草案](../../integration/preprocess_apb_cdc_contract_draft_20260711.md)：冻结特征快照、配置提交和 CPU-to-OSD 的交接语义；不分配地址、不修改 RTL。
- [Priority 3 myCobot 板上 CPU 迁移设计](priority3_mycobot_cpu_migration_design.md)：机械臂控制从 PC 调试流程迁移到板上 CPU 的设计约束。
- [myCobot 板上调试执行计划](mycobot_board_debug_execution_plan_20260712.md)与[操作员 SOP](mycobot_board_bringup_operator_sop_20260712.md)：具体基线、允许范围和动作门以 `CURRENT_STATE.md` 为准。
- [myCobot 机械臂上板控制推进方案](mycobot_arm_board_control_advancement_plan_20260715.md)：状态：待 Codex 审核。按 G4→G11 顺序分层解锁，先做阶段 B（纯软件完善：协议真值表、模拟故障注入、回环测试套件），再做阶段 A（G4 攻坚：审查工程、修 PNR/outpad、闭合 SoC/BSP/PNR/部署链）。本方案不解除任何硬件或机械臂安全门槛。
- [2026-07-17 无板调试强 Goal 包](no_board_debug_strong_goals_20260717.md)：两个可独立闭环的 Host-only Goal（事务时间边界、离线 presubmit），明确验收命令、范围和停止条件。
- [2026-07-17—2026-07-21 单摄项目收敛与板卡/协作依赖执行方案](board_dependent_execution_plan_20260717.md)：基于实时 Git/G1/G2/源码审计，把当前批次操作包、USER2/PC/UART0、既有 APB MAGIC、F1 原子批次和 20 轮验收拆成 qzs/libaoxun/wsc 可交接任务卡；UART2/机械臂继续 HOLD。
- [Type-C UART1 视频调试方案](typec_uart1_video_debug_plan.md)：Type-C 调试观测链路的规划和接口边界。

## 使用规则

- 开始实施前先检查根目录 `CURRENT_STATE.md` 和最新 handoff。
- 涉及 `top.v`、时钟/复位、CDC、QCRV32/APB、`mem_test.xml`、`.peri.xml`、IP `settings.json` 或 `constrain.sdc` 时，按 `AGENTS.md` 提交 Codex Review Packet。
- 不把本目录中的占位地址、旧通道命名或规划状态直接当作当前硬件事实。
- 方案完成首个可验证 checkpoint 后，应把新事实、风险和下一步同步到 `CURRENT_STATE.md` 或对应任务日志。
