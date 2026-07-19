# 团队合并每日讲解指南（三轨学习系统）

本目录用于存放每次个人分支合并后自动生成的日常三轨学习指南。这些指南旨在帮助团队成员（A/B/C 三位不同职责的角色）在 **10~15 分钟** 内快速掌握今日合并的核心改动与避坑经验，并给出推荐的自学切入点。

## 📁 目录结构

* [daily_guide_template.md](daily_guide_template.md)：标准卡片式学习指南模板。
* [no_board_mycobot_maintenance_learning_brief_20260717.md](no_board_mycobot_maintenance_learning_brief_20260717.md)：供 Gemini 扩写的无板机械臂/项目维护学习简报；当前 Gate 以 `CURRENT_STATE.md` 为准。
* `daily_sync/`：按日期和合并分支归档的子目录。例如：
  * [daily_sync/2026-07-20_merge_codex_qzs-wsc-p0a-p1-integration-20260720/](daily_sync/2026-07-20_merge_codex_qzs-wsc-p0a-p1-integration-20260720/)：qzs 与 WSC P0-A/P1 Host 固定 SHA 的双人专用集成；与三人分支明确区分，板级和 libaoxun UART 实验不在本次合并内。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-20_merge_codex_qzs-wsc-p0a-p1-integration-20260720/guide_fpga_vision_front.md)：冻结硬件面与板级 READY 边界（角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-20_merge_codex_qzs-wsc-p0a-p1-integration-20260720/guide_cpu_arm_control.md)：P0-A canary/有界 UART 与 P1 事务（角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-20_merge_codex_qzs-wsc-p0a-p1-integration-20260720/guide_system_integration.md)：双人命名、固定 SHA 与未来三方 Gate（角色 C）。
  * [daily_sync/2026-07-19_merge_codex_qzs-wsc-libaoxun-integration-20260718/](daily_sync/2026-07-19_merge_codex_qzs-wsc-libaoxun-integration-20260718/)：合并 WSC `13419d9`、libaoxun `72cc281` 与 qzs-final `018ced2` 后的 Goal 1/2 固定 SHA 集合指南；严格区分 I0-BUILD 与板级 I0-SMOKE。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-19_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_fpga_vision_front.md)：UART1 Hard SoC 原子批次、管脚与证据分层（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-19_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_cpu_arm_control.md)：严格 Host 门、同批 UART1 Hello 与 ARM 禁用（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-19_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_system_integration.md)：旧 BLOCKED 时间线、固定 SHA 合并与下一 Gate（面向角色 C）。
  * [daily_sync/2026-07-18_merge_codex_qzs-wsc-libaoxun-integration-20260718/](daily_sync/2026-07-18_merge_codex_qzs-wsc-libaoxun-integration-20260718/)：WSC、QZS PR #12 与 libaoxun PR #13 的固定 SHA 本地集成指南；保留任务二、classifier 和板级证据阻塞标注。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-18_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_fpga_vision_front.md)：双批次 manifest、硬件证据和 I1 边界（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-18_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_cpu_arm_control.md)：idle-drain ACK、任务二 WAIT 与 Host 结果（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-18_merge_codex_qzs-wsc-libaoxun-integration-20260718/guide_system_integration.md)：固定 SHA、失败标注和队友拉取边界（面向角色 C）。
  * [daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/](daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/)：7 月 8 日合并分支时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_fpga_vision_front.md)：FPGA与视频前端（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_cpu_arm_control.md)：嵌入式 CPU 控制与通信（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-08_merge_dev_pc_arm_v2.12/guide_system_integration.md)：机械臂控制调试与项目系统维护更新（面向角色 C）。
  * [daily_sync/2026-07-10_merge_cpu_fpga_integration/](daily_sync/2026-07-10_merge_cpu_fpga_integration/)：7 月 10 日合并 CPU-FPGA 双系统集成分支时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-10_merge_cpu_fpga_integration/guide_fpga_vision_front.md)：FPGA与视频前端（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-10_merge_cpu_fpga_integration/guide_cpu_arm_control.md)：嵌入式 CPU 控制与通信（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-10_merge_cpu_fpga_integration/guide_system_integration.md)：机械臂控制调试与项目系统维护更新（面向角色 C）。
  * [daily_sync/2026-07-12_merge_dev_wsc6090-CPU/](daily_sync/2026-07-12_merge_dev_wsc6090-CPU/)：7 月 12 日合并 CPU 四任务 matcher 与事务锁分支时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-12_merge_dev_wsc6090-CPU/guide_fpga_vision_front.md)：FPGA 目标寄存器与事件接口约束（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-12_merge_dev_wsc6090-CPU/guide_cpu_arm_control.md)：四任务真源、幂等锁与错误锁存（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-12_merge_dev_wsc6090-CPU/guide_system_integration.md)：机械臂七阶段安全门（面向角色 C）。
  * [daily_sync/2026-07-13_merge_team-integration/](daily_sync/2026-07-13_merge_team-integration/)：7 月 13 日整合两位队友分支、个人机械臂证据与双层 CPU 控制器时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-13_merge_team-integration/guide_fpga_vision_front.md)：合成源、生产构建开关与真实摄像头验收边界（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-13_merge_team-integration/guide_cpu_arm_control.md)：逐轮状态机与观察事务层的职责拆分（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-13_merge_team-integration/guide_system_integration.md)：J4/底座风险、板上安全门与低冲突 Git 维护（面向角色 C）。
  * [daily_sync/2026-07-14_merge_dev_wsc6090-CPU/](daily_sync/2026-07-14_merge_dev_wsc6090-CPU/)：7 月 14 日吸收队友 CPU 安全补测、Fable 16 位事件契约与个人 UART2 证据时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-14_merge_dev_wsc6090-CPU/guide_fpga_vision_front.md)：production/debug 构建边界与 PNR 阻塞（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-14_merge_dev_wsc6090-CPU/guide_cpu_arm_control.md)：16 位序号、显式 ACK 与 `arm_busy` 安全门（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-14_merge_dev_wsc6090-CPU/guide_system_integration.md)：J52 门禁、分层验收与可追溯合并（面向角色 C）。
  * [daily_sync/2026-07-14_merge_mycobot-g0-g3-bringup-20260714/](daily_sync/2026-07-14_merge_mycobot-g0-g3-bringup-20260714/)：7 月 14 日针对分支 `mycobot-g0-g3-bringup-20260714` 上的 G1–G3 检查点和官方串口协议微调时生成的讲解指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-14_merge_mycobot-g0-g3-bringup-20260714/guide_fpga_vision_front.md)：PNR 约束缺陷、SoC PLL 冲突与 APB 隔离基线（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-14_merge_mycobot-g0-g3-bringup-20260714/guide_cpu_arm_control.md)：16位回绕去重、`arm_busy` 动作屏蔽、编译矩阵 ELF 修复与 QEMU 入口纠偏（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-14_merge_mycobot-g0-g3-bringup-20260714/guide_system_integration.md)：官方协议无应答事实、两维正交构建降级与人机职责边界划定（面向角色 C）。
  * [daily_sync/2026-07-14_merge_single-camera_mycobot-main/](daily_sync/2026-07-14_merge_single-camera_mycobot-main/)：7 月 14 日将 myCobot G0-G3 软件门与单摄候选工程依次合入本地 main 后生成的最终三轨指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-14_merge_single-camera_mycobot-main/guide_fpga_vision_front.md)：候选工程身份、白平衡除零与 M0 板级复现门（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-14_merge_single-camera_mycobot-main/guide_cpu_arm_control.md)：可移植工具链发现、结构桥与 NOT_FOR_FLASH 边界（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-14_merge_single-camera_mycobot-main/guide_system_integration.md)：语义冲突处理、分层证据与队友同步（面向角色 C）。
  * [daily_sync/2026-07-15_merge_dev_libaoxun688/](daily_sync/2026-07-15_merge_dev_libaoxun688/)：7 月 15 日合并 lib 的 HDMI-only 单摄 FPGA 候选后生成的三轨指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-15_merge_dev_libaoxun688/guide_fpga_vision_front.md)：DSI 解耦、真实时钟 CDC 约束与显示校正证据边界（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-15_merge_dev_libaoxun688/guide_cpu_arm_control.md)：显示支路与 CPU 识别真源分离、APB 接入待办（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-15_merge_dev_libaoxun688/guide_system_integration.md)：固定 SHA、证据保质期与候选路线升格门（面向角色 C）。
  * [daily_sync/2026-07-15_merge_dev_libaoxun688_e129885_and_dev_wsc6090-CPU_c165144/](daily_sync/2026-07-15_merge_dev_libaoxun688_e129885_and_dev_wsc6090-CPU_c165144/)：7 月 15 日在个人备份之上本地整合单摄 M2 feature/CPU Host 候选与 CPU→OSD 结果打包设计门，并发现 Hard SoC 系统真源未随固定提交同步后生成的三轨指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-15_merge_dev_libaoxun688_e129885_and_dev_wsc6090-CPU_c165144/guide_fpga_vision_front.md)：被动特征旁路、framebuffer 可观测性与 Hard SoC 真源缺口（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-15_merge_dev_libaoxun688_e129885_and_dev_wsc6090-CPU_c165144/guide_cpu_arm_control.md)：结果语义、原子提交、CDC 与 Wire ABI 未定边界（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-15_merge_dev_libaoxun688_e129885_and_dev_wsc6090-CPU_c165144/guide_system_integration.md)：固定 SHA、语义解冲突与机械臂安全门（面向角色 C）。
  * [daily_sync/2026-07-16_merge_dev_libaoxun688-hard-soc-source-sync-20260716/](daily_sync/2026-07-16_merge_dev_libaoxun688-hard-soc-source-sync-20260716/)：7 月 16 日合并单摄 Hard SoC 可复现真源、最小 BSP 与片上 RAM UART0 Hello，并将旧“源码缺失/HOLD”更新为“离线构建通过、板上执行待验证”后生成的三轨指南。
    * [guide_fpga_vision_front.md](daily_sync/2026-07-16_merge_dev_libaoxun688-hard-soc-source-sync-20260716/guide_fpga_vision_front.md)：Hard SoC 真源、DDR 配置唯一驱动与证据批次边界（面向角色 A）。
    * [guide_cpu_arm_control.md](daily_sync/2026-07-16_merge_dev_libaoxun688-hard-soc-source-sync-20260716/guide_cpu_arm_control.md)：片上 RAM ELF、fail-closed BSP 与 UART0 115200 隔离门（面向角色 B）。
    * [guide_system_integration.md](daily_sync/2026-07-16_merge_dev_libaoxun688-hard-soc-source-sync-20260716/guide_system_integration.md)：快速合并、过时状态清理与硬件安全门（面向角色 C）。

## 📑 专项技术方案大白话解读

除了日常分支合并的同步指南，本项目还针对重大的架构设计与拿分方案提供了大白话通俗化解读，帮助非专业背景的决策者低成本看懂技术决策并回答评审决策清单：

* [competition_score_maximization_explanation_guide.md](competition_score_maximization_explanation_guide.md)：决赛最大化得分总控方案的大白话通俗解读与决策面板。
* [mycobot_cpu_board_bringup_explanation_guide.md](mycobot_cpu_board_bringup_explanation_guide.md)：myCobot 机械臂上板主实施方案的大白话通俗解读与分级门禁指南。
* [m2_preflight_and_risk_guide.md](m2_preflight_and_risk_guide.md)：单摄 M2 预飞行安全门与物理推进风险白话指南。

## 🤝 AI 协作与三岗位学习包（2026-07-17）

* [ai_collaboration_roles_20260717/](ai_collaboration_roles_20260717/)：面向 qzs、wsc、libaoxun 的 Agent 使用与岗位补课指南；以 `competition_project_single_camera/` 的 bring-up 任务为场景，强调接口优先、证据分层、原子批次和安全 Gate。
  * [guide_agent_common.md](ai_collaboration_roles_20260717/guide_agent_common.md)：全员通用的方案讨论—执行—验收工作流。
  * [guide_qzs_integration_owner.md](ai_collaboration_roles_20260717/guide_qzs_integration_owner.md)：qzs 的集成负责人、机械臂安全与仓库治理学习重点。
  * [guide_wsc_cpu_control.md](ai_collaboration_roles_20260717/guide_wsc_cpu_control.md)：wsc 的 CPU、分类、状态机与 ABI/MMIO 学习重点。
  * [guide_libaoxun_fpga_vision.md](ai_collaboration_roles_20260717/guide_libaoxun_fpga_vision.md)：libaoxun 的视频前端、Hard SoC、CDC 与构建证据学习重点。

---

## 🚀 交互触发生成机制

在后续开发中，每当完成一次个人分支合并，您可以在对话框中直接使用如下指令唤醒我生成指南：

> **“我刚刚完成了分支合并 `[分支名]`。今日的主要成果是 `[这里简要写一两句话，例如：调通了 PC 端 retry 熔断逻辑]`。请为我读取最近的 Handoff 文件和 Git Diff，生成今天的 3 份讲解指南。”**

我将在接收到命令后：
1. 运行 `git diff` / `git log` 分析最近的更改；
2. 结合 `CURRENT_STATE.md` 中的增量条目与最新的 Handoff 文件（如 `SESSION_HANDOFF.md` / `debug_records/`）；
3. 解析出业务逻辑，套用 `daily_guide_template.md` 格式；
4. 按“日期+合并分支名”规范（如 `daily_sync/YYYY-MM-DD_merge_[分支名]/`）输出三份 markdown 指南文档。
