# 学习指南：嵌入式 CPU 与通信控制 - 两层控制器各守一道门
> 生成日期: 2026-07-13 | 对应分支合并: `origin/dev/libaoxun688@7a51cba` + `origin/dev/wsc6090-CPU@dcd9584`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：同名控制器不是二选一，而是上下两道门
* **改动背景与问题**：两个分支都新增 `round_controller`，但一个负责“观察—判定—ACK”的单笔事务，另一个负责 APPLY/PLACE/REMOVE、锁存、SKIP/GRAB、等待机械臂和故障恢复。直接选 ours/theirs 会丢掉一半能力。
* **源码剖析**：[competition_round_transaction.c](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/competition_round_transaction.c#L12-L96) 保存轻量事务层；[round_controller.h](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/include/round_controller.h#L11-L86) 定义正式逐轮状态、事件、输入输出；[round_controller.c](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/round_controller.c#L69-L223) 推进整轮状态机。
* **经验总结**：可以把它想成机场的两道门：事务层是“登机牌核验”，正式控制器是“从值机、安检到登机的全流程调度”。两者都重要，但不能共用同一个柜台名称和函数符号。

### 📌 核心点二：锁存、序号和有界等待共同防止重复动作
* **改动背景与问题**：机械臂动作不能因为同一帧重复到达、事件重放或等待无响应而重复触发。
* **源码剖析**：[competition_round_transaction.c](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/competition_round_transaction.c#L43-L78) 锁存首个最终判断并校验 ACK 序号；[round_controller.c](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/round_controller.c#L93-L223) 只在正确状态发出一次抓取请求，并通过超时/故障进入可解释终态。
* **经验总结**：这是“留号排队 + 一次性取件码”。不在柜台死等，也不能拿同一张取件码领两次物品。

### 📌 核心点三：验证通过的是 Host/Mock，不是板级闭环
* **源码剖析**：[CURRENT_STATE.md](file:///d:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L122-L129) 记录本次 `795/795` Host 回归和 8 个源文件 RISC-V compile-only 结果。
* **经验总结**：飞行模拟器里的 795 次安全降落很有价值，但真正跑道的 APB、CDC、OSD、UART 和机械臂仍需逐门放行。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：不要把事务 ACK 与机械臂完成 ACK 混成一个信号；前者确认结果被消费，后者确认物理动作完成，超时与恢复策略不同。
* **通用方法论**：接口冲突先按职责拆层，再改名消除符号碰撞，最后分别保留原测试。这样比覆盖一方实现更容易追责和回归。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：QCRV32/JTAG/AXI-APB 例程、生成 `soc.h` 与 UART/中断驱动示例。
  * **应用与意义**：把 Host 状态机接入正式寄存器与板上事件，同时保持 `ARM_DISABLED` 安全门。

* **拓展基础知识**：
  * **推荐学习内容**：层次状态机、幂等事件处理、序号回绕、单调时钟超时和故障注入。
  * **应用与意义**：设计 20 轮不会死锁、不会重复抓取、能解释失败原因的事务系统。
