# 学习指南：嵌入式 CPU 与通信控制 - 16位事件契约、`arm_busy` 安全门与 QEMU/ELF 严密性漏洞修复
> 生成日期: 2026-07-14 | 对应分支合并: codex/mycobot-g0-g3-bringup-20260714@d433bca
> 预计阅读时间: 10-15 分钟 | 面向角色: B (嵌入式CPU与通信控制)

---

## 1. 今日最核心的改动与经验

### 📌 核心点一: 16-bit `event_seq`/ACK 契约与 `arm_busy` 安全防线
* **改动背景与问题**：
  原先的代码中，CPU 仅以简单的 8 位事件序号和简易 matcher 结果控制机械臂。这使得在事件号突变或回绕（如 `255 -> 0`）时，容易引发误判；而且当机械臂正在执行动作（`arm_busy=1`）时，若此时由于异常或者误触发再次发送了 grab 请求，可能导致机械臂状态发生死锁或硬件碰撞。
* **源码剖析**：
  在本次集成中，已将事件消费与确认机制统一重构为 16-bit [round_controller.c](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/round_controller.c) 的事件序号与 ACK ABI 契约，支持 `65535 -> 0` 半范围回绕判定（即利用无符号差值 `(uint16_t)(seq - last)` 且 `delta < 32768` 视为新事件，其余一律拒绝或忽略）。
  同时，在 `round_controller.c` 中加入了 `arm_busy` 发起动作安全门：当一轮判定为需要抓取（`GRAB`）且已启动机械臂，但监测到当前 `arm_busy==1` 时，强制拦截动作发送，直接进入 `ROUND_DONE` 并报告 `REASON_ARM_NOT_READY`。
* **经验总结（大白话比喻）**：
  - **事件防重与回绕**：这就像是餐馆的**“点餐票号”**。柜台只收“号比上一次大”的餐票。如果有人拿过期的旧票或者重票来领餐（重复或过期 `event_seq`），系统一律拒收；当票号发完从 `65535` 回绕到 `0` 时，柜台能通过“半范围判定”认出这是新的一轮发票，而不是过期旧票。
  - **`arm_busy` 安全门**：这类似于**“电梯运行中的按钮屏蔽”**。当电梯还在高速上下行运行中（`arm_busy==1`），即使外面有人强行狂按“开门”或“关门”（发起新的 grab 动作），中途控制器也会出于安全考虑无条件屏蔽此请求（零动作请求，报告设备忙），只有等到电梯完全停稳（IDLE/DONE）后才能接收下一次控制。

### 📌 核心点二: RISC-V 交叉编译 ELF 符号回收与编译矩阵断路
* **改动背景与问题**：
  在只读复核中发现，编译矩阵的四种配置（`competition/arm_bringup × disabled/simulated`）中，`arm_bringup + disabled` 构建未能通过 ELF 门（3/4 PASS，1/4 FAIL）。
* **源码剖析**：
  这是因为在 `disabled` 模式下，代码中没有可达路径调用 `round_controller_tick` 模块，导致在链接阶段链接器使用 `--gc-sections` 垃圾回收参数时，直接将整个核心的 `round_controller` 及其相关符号回收清空，触发了 [verify_arm_elf.ps1 (line 37)](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/build_tools/verify_arm_elf.ps1#L37) 中对必要符号的精确检查限制。
* **经验总结**：
  我们绝不能为了过门而放宽 ELF 符号门限制。正确的修复方案是：在 `disabled` 的 bring-up 中，也增加一段确定性的 **“20 轮零请求自检逻辑”**，从而显式地保留对相关状态机符号的引用，保证 4/4 编译矩阵全部合法通过，以此来加固编译安全性。

### 📌 核心点三: QEMU 入口符号不一致与超时生效缺陷
* **改动背景与问题**：
  运行 `run_arm_runtime_qemu.ps1` 脚本时虽然显示 PASS，但这属于“条件通过”。
* **源码剖析**：
  链接脚本 [scratchpad.lds (line 3)](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/tests/scratchpad.lds#L3) 中指定了程序入口为 `ENTRY(_startup)`，但是 QEMU 的测试启动汇编 [qemu_test_startup.S (line 2)](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/tests/qemu_test_startup.S#L2) 却只导出了 `_start` 符号。这导致链接器在找不到 `_startup` 时，发出了 Warning 并随机选择了一个默认入口。虽然 QEMU 幸运地运行通过，但极其危险。此外，脚本中的 `TimeoutSeconds` 并未真正绑定到底层进程监控上，一旦固件挂死，QEMU 会无限挂起。
* **经验总结（大白话比喻）**：
  这就像是**“快递收件人名字写错了，虽然邮差认出地址强行送到了，但留下了退件隐患”**。由于 `ENTRY` 符号和真实汇编导出名称对不上，链接器处于“猜”的阶段。一旦编译器优化级别或文件排列发生变化，入口就会完全错乱，导致程序直接跑飞崩溃。下一步必须严格统一 `ENTRY` 入口符号，并为 QEMU 挂载真正的超时保护。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：
  在嵌入式主循环中，不要使用临时的循环递增计数 `competition_now_ms++` 来充当毫秒延时。在 [main.c (line 269)](file:///d:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/main.c#L269) 中，当前的 legacy matcher 和这种计数方式仅仅是安全的“结构桥梁”，它无法反映真实硬件的延迟。
* **通用方法论**：
  **“无受审物理时基，则不验证超时逻辑”**。在正式引入 CLINT/mtime 或硬件定时器之前，我们应当在 QEMU 和 Host 上采用显式的 “test clock”（模拟时基接口），并且在日志中将当前的 G2 阶段严格标记为 “structural bridge”，不冒充实际的逐轮闭环。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：查阅 `赛方提供材料/RISC-V例程/` 中提供的 QCRV32 软件开发包（SDK）、启动代码 `startup.S`、以及 `linker` 链接配置文件。
  * **应用与意义**：熟悉 RISC-V 从硬复位向量进入 `_start`、初始化 `.data`/`.bss` 段、分配栈空间以及进入 `main` 函数的完整裸机引导过程，彻底纠正当前 `_startup` 的符号 warning 问题。

* **拓展基础知识**：
  * **推荐学习内容**：自学 GNU Linker 脚本语言（LD）和汇编指令中 `ENTRY`、`KEEP`、`PROVIDE` 关键字的用法，以及用 Python/PowerShell 异步监控后台子进程并加挂带界限超时监控的方法。
  * **应用与意义**：在 Host 和 QEMU 仿真中构建真正稳健的非阻塞超时测试机制，确保在未来的 G4-G5 阶段上板部署时，能自动防范死循环卡死现象。
