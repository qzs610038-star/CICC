# 学习指南：嵌入式 CPU 与通信控制 - 严格 Host 门与 UART1 Hello
> 生成日期: 2026-07-19 | 对应分支合并: WSC `13419d9` + libaoxun `72cc281`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一: 修复测试入口，不能把警报器静音

* **改动背景与问题**：旧 MSVC `/W4 /WX` 入口因 `C4127` 在编译阶段停止，测试可执行文件没有运行。WSC 把 `CHECK` 的常量循环包装改成真实的 `record_check()` 调用。
* **源码剖析**：[test_single_camera_classifier.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/tests/test_single_camera_classifier.c#L8-L17) 保留每次断言计数、失败行号和表达式文本。合并树 fresh 结果为 classifier `54/54`、F1 `213/213`、adapter `33/33`、runtime `648/648`。
* **经验总结**：这不是把烟雾报警器电池拔掉，而是修正误触发的安装角度；`/W4 /WX` 仍开着，真正起火时仍会报警。

### 📌 核心点二: Hello 只能消费同批 `SYSTEM_UART_1_*`

* **改动背景与问题**：旧 UART0 基址和 ELF 不能迁移到 UART1。新的 Hello 直接使用生成头文件的 UART1 宏。
* **源码剖析**：[uart1 Hello main.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/embedded_sw/uart1_hello_onchip/src/main.c#L23-L40) 计算分频并访问 `SYSTEM_UART_1_IO_CTRL`；回显循环见 [main.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/embedded_sw/uart1_hello_onchip/src/main.c#L59-L62)。这避免硬编码旧 UART0 地址。
* **经验总结**：生成的 `soc.h` 像城市最新地铁图。拿旧图硬猜新站位置，程序也许能编译，却可能把数据写进完全错误的寄存器。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：Host `648/648` 只覆盖 fake/Host seam；它不证明 RISC-V ELF 在板上取指，也不证明 MMIO、UART1 或 APB 通路。当前真实动作门仍由 [single_camera_runtime.h](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/include/single_camera_runtime.h#L56) 的 `SC_RUNTIME_ARM_ENABLED=0` 锁住。
* **通用方法论**：编译器发现、命令 argv、测试退出码和断言计数都应进入证据包。runner 自己 PASS 不够，必须证明被调用的 C 测试真的运行并把失败码传出来。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity Sapphire SoC BSP、UART driver 参数编码、片上 RAM 链接布局。
  * **应用与意义**：理解 `soc.h`、链接脚本、启动代码和 UART 外设为什么必须同批。

* **拓展基础知识**：
  * **推荐学习内容**：C 测试宏的副作用、编译器 warning-as-error、MMIO 的 `volatile` 与轮询超时。
  * **应用与意义**：在不牺牲严格门的前提下修复可移植性，并为真实板级 fail-closed 行为打基础。
