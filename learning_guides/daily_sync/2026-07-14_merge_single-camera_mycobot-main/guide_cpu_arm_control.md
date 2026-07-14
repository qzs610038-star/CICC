# 学习指南：嵌入式 CPU 与通信 - 软件门通过不等于可烧录
> 生成日期: 2026-07-14 | 对应分支合并: `b48973b + edd5328 -> main@45c790c`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：工具路径从个人电脑移出源码
* **改动背景与问题**：构建脚本写死某台电脑的 Efinity、QEMU 或 Visual Studio 路径，队友拉取后会立刻失效。
* **源码剖析**：[build_arm_profile.ps1](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/build_tools/build_arm_profile.ps1#L10-L33) 使用参数、`EFINITY_RISCV_TOOLCHAIN`、`EFINITY_HOME` 与 PATH；[run_arm_runtime_qemu.ps1](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/tests/run_arm_runtime_qemu.ps1#L1-L33) 同样显式发现 QEMU；Host runner 接受 `VCVARS64_PATH`。
* **经验总结**：脚本像公共充电线，接口可以约定，插座位置不能焊死在某个人桌下。

### 📌 核心点二：`main` 只有结构桥，没有虚构事件源
* **改动背景与问题**：CPU 必须经唯一 `round_controller -> arm_runtime` 网关，但当前没有正式操作员事件和单调时基。
* **源码剖析**：[main.c](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/main.c#L260-L268) 初始化控制器和 runtime；[main.c](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/main.c#L358-L380) 明确不从 legacy matcher 合成事件或直接动作请求。
* **经验总结**：这是“先铺带闸门的轨道，但不开假信号灯”。结构可以提前接好，事件真源和时基不能脑补。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：四种 RISC-V 组合全部带 `NOT_FOR_FLASH`；它们验证链接边界和 UART2 排除，不是正式固件。
* **通用方法论**：将应用入口 `competition/arm_bringup` 与后端 `disabled/simulated` 正交组合，故障时可逐层熔断，而不把测试通道误接到真实机械臂。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：QCRV32 BSP、Efinity 生成 `soc.h`、UART 与 APB 文档。
  * **应用与意义**：G4 需要同次生成的真实时钟、复位、地址和 linker/startup 证据。
* **拓展基础知识**：
  * **推荐学习内容**：可复现构建、ELF 符号白名单和 fail-closed 设计。
  * **应用与意义**：理解为什么 `-BoardBuild` 要在创建产物前失败，为什么测试 ELF 不能被误烧录。
