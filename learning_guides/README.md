# 团队合并每日讲解指南（三轨学习系统）

本目录用于存放每次个人分支合并后自动生成的日常三轨学习指南。这些指南旨在帮助团队成员（A/B/C 三位不同职责的角色）在 **10~15 分钟** 内快速掌握今日合并的核心改动与避坑经验，并给出推荐的自学切入点。

## 📁 目录结构

* [daily_guide_template.md](daily_guide_template.md)：标准卡片式学习指南模板。
* `daily_sync/`：按日期和合并分支归档的子目录。例如：
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

## 📑 专项技术方案大白话解读

除了日常分支合并的同步指南，本项目还针对重大的架构设计与拿分方案提供了大白话通俗化解读，帮助非专业背景的决策者低成本看懂技术决策并回答评审决策清单：

* [competition_score_maximization_explanation_guide.md](competition_score_maximization_explanation_guide.md)：决赛最大化得分总控方案的大白话通俗解读与决策面板。

---

## 🚀 交互触发生成机制

在后续开发中，每当完成一次个人分支合并，您可以在对话框中直接使用如下指令唤醒我生成指南：

> **“我刚刚完成了分支合并 `[分支名]`。今日的主要成果是 `[这里简要写一两句话，例如：调通了 PC 端 retry 熔断逻辑]`。请为我读取最近的 Handoff 文件和 Git Diff，生成今天的 3 份讲解指南。”**

我将在接收到命令后：
1. 运行 `git diff` / `git log` 分析最近的更改；
2. 结合 `CURRENT_STATE.md` 中的增量条目与最新的 Handoff 文件（如 `SESSION_HANDOFF.md` / `debug_records/`）；
3. 解析出业务逻辑，套用 `daily_guide_template.md` 格式；
4. 按“日期+合并分支名”规范（如 `daily_sync/YYYY-MM-DD_merge_[分支名]/`）输出三份 markdown 指南文档。
