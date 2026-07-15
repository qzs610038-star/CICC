# 学习指南：FPGA 与视频前端 - 被动特征旁路与 Hard SoC 真源缺口
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@e129885` + `dev/wsc6090-CPU@c165144`
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：先装“只读仪表”，不让它接管视频链

* **改动背景与问题**：识别链需要 ROI、颜色面积、亮度和包围框等统计量，但在 APB/CDC 尚未定版时直接发布数据，会把实验接口误当成正式通道。
* **源码剖析**：`feature_stats_tap` 只观察 ch0 双像素 RGB，统计计数、亮度和 bbox，见 [feature_stats_tap.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/feature_stats/feature_stats_tap.v#L1-L47)；顶层把 `i_capture_enable` 固定为 `1'b0`，所有结果接 `_unused`，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1390-L1435)。
* **经验总结**：这像先在流水线上装摄像头和计数器，但不把它接到放行闸门。能观察结构，不等于已经拥有合法的数据发布链路。

### 📌 核心点二：把 framebuffer 健康度变成可见信号

* **改动背景与问题**：画面偶发异常时，仅看 HDMI 很难区分帧长度不稳与读 FIFO 欠流。
* **源码剖析**：`frame_buffer` 新导出 `o_frame_stable` 与 `o_fifo_rd_underflow`，见 [frame_buffer.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/framebuffer/frame_buffer.v#L34-L38)；ch0 将稳定状态和欠流锁存映射到 LED20/LED21，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1027-L1035)。
* **经验总结**：这像给水管增加“压力正常”和“曾经断流”两盏灯；现场不必只凭肉眼猜画面故障发生在哪一段。

### 📌 核心点三：说明写了 Hard SoC，不等于固定提交带齐系统真源

* **改动背景与问题**：队友交接记录了外部联合工程 PNR、STA、CDC 和板级视频证据，但 `e129885` 没有同步交接要求的 Hard SoC IP、BSP/Hello 入口和资源真源，现有 XML/SDC 也未满足说明列出的静态条件。
* **源码剖析**：状态索引记录 `auto_instantiation=on`、Hard SoC IP/BSP 缺失和旧 AXI1/PLL 约束仍有效的 HOLD 结论，见 [CURRENT_STATE.md](file:///D:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L38-L48)。交接文件自己的 Gate A-D 与停止条件见 [m2 handoff](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/docs/review_packets/m2_single_camera_soc_branch_merge_handoff_20260715.md#L233-L279)。
* **经验总结**：验收报告像食品批次证书，固定提交像实际到货箱单；证书写得再完整，箱里缺了关键原料也不能入库放行。

> **风险标注：来源联合副本已实测，当前仓库 Hard SoC 真源同步 HOLD。** 在系统文件补齐、Check Design、Map/PNR/STA/CDC 与匹配 bitstream 板级回归完成前，不把 `f353207` 写成 Hard SoC 候选闭环。

## 2. 可迁移的系统级工程经验

* **避坑警示**：不要把固定为关闭的 feature tap 写成“特征链已接通”；不要因输出未使用就忽略其时钟、复位和资源影响。
* **通用方法论**：按“被动旁路 → 独立 RTL 测试 → 工程登记 → PNR/STA/CDC → 匹配 bitstream → 板级视频”逐门升级，每一门只证明自己的范围。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity Interface Designer、STA/CDC 报告与 QCRV32 Hard SoC 生成资料。
  * **应用与意义**：确认 USER1/USER2、PLL、DDR、UART0 与视频资源不会在重新生成时互相抢占。
* **拓展基础知识**：
  * **推荐学习内容**：视频帧边界检测、饱和计数器、异步复位同步释放和 bundled-data CDC。
  * **应用与意义**：理解统计旁路为何能保持只读，以及何时必须通过握手才能跨域发布快照。
