# 学习指南：系统集成与机械臂安全 - 固定 SHA、分层证据与本地审核门
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@e129885` + `dev/wsc6090-CPU@c165144`
> 预计阅读时间: 10-15 分钟 | 面向角色: C

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：先把自己的行李寄存，再接队友两列火车

* **改动背景与问题**：活动工作区已有 myCobot 板控改动，直接在 main 合并会让回退、审查和归因纠缠。
* **源码剖析**：本轮先以 `e1041cf` 固定个人备份，再在独立本地分支精确 cherry-pick `c165144`、合并 `e129885`；当前状态条目记录了两个子系统互不扩权的边界，见 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L30-L60)。
* **经验总结**：像先把自己的行李贴签寄存，再让两列火车进同一站台；哪一件货来自谁、出现问题退到哪里都清楚。

### 📌 核心点二：冲突要合并事实，不能投票选整页

* **改动背景与问题**：`CURRENT_STATE.md` 同一区域同时新增 myCobot 软件门与单摄 Hard SoC 状态，盲选 ours/theirs 会丢掉另一条真实路线。
* **源码剖析**：解决后同时保留 myCobot B1-B3/A0 条目和单摄 M2 条目，并把 Hard SoC 真源缺失标为 HOLD、UART0 执行标为 NOT VERIFIED，见 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L30-L49)。
* **经验总结**：冲突不是两份答案二选一，而像把两张值班表按岗位和时间重新排版；事实都保留，过期口径要改写。

### 📌 核心点三：机械臂安全门不随“设计批准”自动打开

* **改动背景与问题**：OSD 结果里出现 arm_state、REQUESTED 等字段，容易让人误以为机械臂通路已经存在。
* **源码剖析**：设计包要求 ARM_DISABLED 下永不产生 `request_arm_grab`，见 [fail-safe](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L340-L375)；UART2 与机械臂仍被明确隔离，见 [ARM_DISABLED 边界](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L481-L498)。
* **经验总结**：OSD 上的“机械臂状态”只是仪表盘格子，不是点火钥匙。电气、协议、断臂回环和动作 Review Packet 未过，动力链始终断开。

> **风险标注：本地合并已形成可审核提交，尚未推送 main。** 固定提交缺少交接要求的 Hard SoC 系统真源，当前为 HOLD；本轮也没有接线、烧录或执行机械臂动作。

## 2. 可迁移的系统级工程经验

* **避坑警示**：Windows 上 `dev/wsc6090-CPU` 与 `dev/wsc6090-cpu` 会发生大小写引用碰撞，审核必须以 `git ls-remote` 的固定 SHA 为准。
* **通用方法论**：备份分支 → 固定提交审查 → 只读 merge-tree → 独立集成分支 → 语义解冲突 → fresh tests → 用户审核 → 最后才进入 main。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：比赛细则的逐轮输出要求、Efinity USER JTAG/Programmer 文档和开发板 UART/J52 原理图。
  * **应用与意义**：把“识别、判断、执行理由”与“是否允许真实动作”分成两套独立验收门。
* **拓展基础知识**：
  * **推荐学习内容**：Git 三方合并、证据可追溯性、固定 SHA 审核和安全关键系统的 fail-safe 状态机。
  * **应用与意义**：让多人并行时既能快速集成，又不会因文档口径扩大而越过硬件安全边界。
