# 学习指南：FPGA 与视频前端 - 双批次证据与 I1 释放边界
> 生成日期: 2026-07-18 | 对应分支合并: `codex/qzs-wsc-libaoxun-integration-20260718`
> 预计阅读时间: 10-15 分钟 | 面向角色: A（FPGA 与视频前端）

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：G1 与 R0 不能共用一张“登机牌”

* **改动背景与问题**：G1 的 `A897.../E5BC...` 与 R0 的 `9F6F.../CD4C...` 来自不同证据批次。混用就像拿 A 航班的登机牌去登 B 航班，文件名相同也不能证明身份相同。
* **源码剖析**：[R0 manifest](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/docs/debug_sessions/r0_current_batch_manifest_20260718.json#L6-L32) 用八项 Git blob、制品 hash 和大小绑定 R0；[CURRENT_STATE](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L47-L58) 把 R0 指定为唯一下一 Gate 活动批次。
* **经验总结**：硬件证据不是“工程大概没变”就能继承，必须逐项证明输入和产物身份。

### 📌 核心点二：本次合并不等于 FPGA 已重新验证

* **改动背景与问题**：三方集成没有改变 XML、peri.xml、SDC、顶层、APB MAGIC、Hard SoC settings 或 Hello 源码。
* **源码剖析**：[CURRENT_STATE](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L45-L58) 明确“不强制重跑 Efinity”，同时把 PR #13 的 USER2/PC/APB 结论标为 `REPORTED / NOT REVERIFIED`。
* **经验总结**：这像封存实验室样品：封条没变，所以不必重新生产；但没有亲手复核原始记录，就不能把别人报告的结果升级成自己的实测 PASS。

## 2. 可迁移的系统级工程经验

* **避坑警示**：真实 I1 单槽、CDC、ACK 和 overrun 仍未实现。runtime 的 idle-drain 只是 Host 语义，属于 **“代码审查通过、运行时未触发”** 的板级风险。
* **通用方法论**：先冻结语义，再冻结 wire ABI；每次硬件输入变化都重新建立完整批次，不跨批继承 bitstream、ELF、slack 或板级现象。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：复习 Hard SoC、APB、CDC 与 Efinity Interface/STA 报告，重点理解生成 `soc.h`、顶层端口和 SDC 必须同批审查。
* **拓展基础知识**：学习异步 FIFO 与 multi-bit snapshot CDC。可把 CDC 理解成“隔洋通话”：时钟不同，必须用握手或 FIFO 防止一句话被拆成两半。
