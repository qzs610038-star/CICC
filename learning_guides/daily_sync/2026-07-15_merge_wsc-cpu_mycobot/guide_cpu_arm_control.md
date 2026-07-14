# 学习指南：嵌入式 CPU 与通信控制 - 跨编译器门和统一结果语义
> 生成日期: 2026-07-15 | 对应分支合并: `dev/wsc6090-cpu@df45b5a` + 集成整改 `dd7fc32`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：环境差异不是借口，而是第二台烟雾报警器

* **改动背景与问题**：wsc 的 MinGW 环境报告 `374/374`、`117/117` PASS；MSVC `/W4 /WX` 却因测试宏局部 `int d` 遮蔽业务变量触发 C4456，并在断言执行前停止。
* **源码剖析**：[run_cpu_result_semantics_host.ps1](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/tests/run_cpu_result_semantics_host.ps1#L75-L105) 为 GCC 增加 `-Wshadow -Werror`，并支持显式选择 MSVC/GCC；验收原因和命令集中在 [验收包](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/wsc_cpu_mycobot_integration_acceptance_20260715.md#L34-L88)。
* **经验总结**：两种编译器像两名安检员：一名查功能票据，另一名擅长发现口袋里的变量遮蔽。结果不一致时先对齐规则，不要让其中一名“闭眼放行”。

补充环境坑：Windows PowerShell 5.1 可能把无 BOM UTF-8 中文注释按本地代码页解码，进而破坏相邻命令解析；本次两份 Host runner 因此保持 ASCII-only。脚本编码本身也属于可复现环境的一部分。

### 📌 核心点二：统一语义层是逐轮事务的唯一翻译员

* **改动背景与问题**：仓库同时存在 matcher、round controller 和 competition tasks 的理由码；直接输出原始 action 容易把“目标但机械臂未就绪”误说成普通不抓。
* **源码剖析**：[main.c](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/app/src/main.c#L383-L403) 通过 `cpu_display_from_round_output()` 校验 action/reason/is_target，再映射成 GRAB/SKIP/NONE。非法组合不会产生执行授权。
* **经验总结**：它像机场的唯一塔台翻译员：跑道、机组和地勤可以使用不同内部术语，但对外放行口令只能从一个经过校验的出口发出。

### 📌 核心点三：Host PASS 之后必须进入目标链接矩阵

* **改动背景与问题**：原规范构建器没有列入 wsc 三个新源文件，Host 测试通过也可能在 RISC-V 链接时漏模块。
* **源码剖析**：[build_arm_profile.ps1](file:///D:/第十届集创赛-雄芯院材料/final_project/cpu/build_tools/build_arm_profile.ps1#L120-L133) 将语义层、适配层和候选主循环 adapter 加入 competition 源码清单；四个 profile/backend 组合均保持 `NOT_FOR_FLASH`。
* **经验总结**：Host 测试像在桌面拼好发动机，目标链接则是确认发动机真的装进指定车架；两者缺一不可。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：禁止用 `/wd4456`、降低 `/WX` 或删除断言适配环境；也不能只编译新增 `.c`，必须验证正式 source manifest 和最终 ELF。
* **通用方法论**：每次 CPU 合并记录 commit、编译器版本、完整 flags、断言数、目标构建矩阵和 manifest。环境差异用“主功能门 + 兼容门”管理。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：QCRV32/Efinity RISC-V GCC 工具链、链接脚本与 BSP 示例。
  * **应用与意义**：理解 Host ABI 与 rv32imac/ilp32 的差异，并核对 `_start`、map、nm 和 objdump 证据。

* **拓展基础知识**：
  * **推荐学习内容**：C 作用域/变量遮蔽、warning policy、可复现构建和 manifest 哈希。
  * **应用与意义**：让“我这里能过”升级为同 commit、同规则、可审计的团队结论。
