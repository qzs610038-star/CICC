# 学习指南：嵌入式 CPU 与通信 - 片上 RAM Hello 安全门
> 生成日期: 2026-07-16 | 对应分支合并: `dev/libaoxun688-hard-soc-source-sync-20260716@0604d33`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：先在院子里点火，不把车直接开上高速

* **改动背景与问题**：正式 CPU 闭环涉及 feature、OSD、UART2 和机械臂，变量太多。最小 Hello 只验证 CPU 是否能从片上 RAM 取指、UART0 是否能发收字符，相当于先在安全支架上点火。
* **源码剖析**：[linker script](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/embedded_sw/efx_hard_soc/bsp/efinix/EfxSapphireSoc/linker/default_i.ld#L7)把程序限定在 `0xF9000000`、16 KiB 片上 RAM；[build.ps1 的 LOAD 审计](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1#L105)拒绝任何越界 ELF。
* **经验总结**：bring-up 要把变量一层层剥掉。先证明取指和串口，再接寄存器业务；先证明静态链路，再讨论机械动作。

### 📌 核心点二：BSP 缺件时必须 fail closed

* **改动背景与问题**：若构建脚本偷偷从某个 IDE workspace 或个人副本寻找 `soc.h`，构建“成功”也不可复现。
* **源码剖析**：[build.ps1 的必需 BSP 检查](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu_bringup/uart_hello_onchip/build.ps1#L23)在缺件时直接报错，并指向同一个 Hard SoC `settings.json` 重生。
* **经验总结**：依赖缺失时的明确失败，比“碰巧从本机找到一份”更安全。它像机场登机口核验证件：少一张就不放行，不允许工作人员凭印象补齐。

### 📌 核心点三：UART0 115200 不是 myCobot 1 Mbps

* **改动背景与问题**：两条链路都叫 UART，最容易被误认为“UART0 通了，机械臂也能接”。实际上它们的端口、波特率和安全等级都不同。
* **源码剖析**：[Hello 固件](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c#L10)固定 115200，并在 [启动横幅与回显循环](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu_bringup/uart_hello_onchip/src/main.c#L51)只访问 UART0。
* **经验总结**：UART0 Hello 是“门卫电话”；UART2/myCobot 是“重型设备控制总线”。门卫电话能响，不代表吊车可以启动。

## 2. 可迁移的系统级工程经验

* **避坑警示**：当前 `ELF_LOAD_AUDIT=PASS` 只证明 ELF 地址正确，不证明 USER2 已连接、CPU 已取指或 UART0 已看到横幅。
* **通用方法论**：每个硬件 Gate 都写成可证伪的小命题：USER2 可连接、横幅完整、输入一个字符原样回显。缺任何一个证据都保留 `NOT VERIFIED`。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：学习 Efinity RISC-V IDE 的 Debug in RAM 流程、JTAG USER TAP 选择和 QCRV32 片上 RAM 地址映射。
* **应用与意义**：能够在不擦写 Flash、不初始化外部 DDR 的前提下完成首次 CPU 执行证据，降低误烧录风险。
* **拓展基础知识**：复习 ELF program header、LOAD 段和 linker script。理解“入口地址正确”为什么仍不等于“板上已经执行”。
