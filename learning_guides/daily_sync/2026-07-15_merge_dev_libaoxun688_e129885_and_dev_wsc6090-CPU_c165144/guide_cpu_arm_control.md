# 学习指南：嵌入式 CPU 与通信控制 - 结果语义先冻结，Wire ABI 后签约
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@e129885` + `dev/wsc6090-CPU@c165144`
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：内部枚举不是 APB 线上的身份证号

* **改动背景与问题**：直接把 C 枚举数值写进寄存器，会让软件重排枚举时悄悄破坏 FPGA/OSD。
* **源码剖析**：设计包只批准逻辑契约，正式 wire ABI、APB/CDC RTL 和 OSD 仍未批准，见 [设计包状态](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L1-L15)；未来必须经显式映射层转换，全部位宽与编码仍为 PROPOSED/TBD，见 [字段定义](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L96-L132)。
* **经验总结**：内部枚举像员工工号，Wire ABI 像对外护照号；二者必须通过登记表转换，不能假设号码碰巧相同。

### 📌 核心点二：整轮结果要“装箱、封箱、交接”

* **改动背景与问题**：颜色、形状、判断和执行理由分多次跨域时，OSD 可能读到半旧半新的混合轮次。
* **源码剖析**：设计冻结 `staging → commit → active`，目标域不得在检测到 toggle 的同周期采样多位总线，见 [原子更新原则](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L150-L183)；返回方向只同步 ACK toggle，由 APB 域维护 confirmed sequence，见 [CDC 返回规则](file:///D:/第十届集创赛-雄芯院材料/final_project/docs/review_packets/cpu_apb_osd_result_packing_design_20260715.md#L195-L248)。
* **经验总结**：像快递装箱：字段先放入 staging，commit 相当于封箱，VSYNC 才把整箱换到展台；不能边装边让观众取。

### 📌 核心点三：单摄 Host F1 先做到“认得、判断、但不动臂”

* **改动背景与问题**：CPU 需要消费特征快照并形成任务结果，但机械臂链路尚未通过安全门。
* **源码剖析**：单摄 F1 目标轮只生成 `SC_DECISION_EXECUTE_ARM_DISABLED`，见 [single_camera_f1.c](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/src/single_camera_f1.c#L108-L125)；接口枚举把该状态独立表达，见 [single_camera_f1.h](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/cpu/include/single_camera_f1.h#L44-L63)。
* **经验总结**：这像驾驶模拟器已经能识别红绿灯并给出决策，但动力输出保险丝仍拔掉；逻辑可测，真实执行被硬门隔离。

> **风险标注：设计包代码审查通过，APB/CDC/OSD 运行时未实现。** `APPROVED BY CODEX2 — DESIGN PACKET ONLY` 不能扩张为正式 ABI 或板级 PASS。

## 2. 可迁移的系统级工程经验

* **避坑警示**：`request_toggle`/`ACK_toggle` 是单比特变化检测，不能与 16-bit `event_seq`/`result_sequence` 的半范围回绕规则混用。
* **通用方法论**：先冻结业务语义和 fail-safe，再由 CPU/FPGA 联合 Review Packet 签 Wire ABI、复位基线和稳定周期，最后才写 RTL。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：QCRV32/APB 外设模板、SoC 生成的 `soc.h` 与 OSD 示例。
  * **应用与意义**：从生成真源确认基址、时钟和复位，避免把 Host 草案地址写成硬件事实。
* **拓展基础知识**：
  * **推荐学习内容**：toggle handshake、bundled-data CDC、序号回绕和显式序列化映射。
  * **应用与意义**：建立可复位、可确认、不会撕裂的 CPU→OSD 事务通道。
