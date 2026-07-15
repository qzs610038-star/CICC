# 学习指南：FPGA 与视频前端 - HDMI-only 单摄候选的时钟、复位与显示校正
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@8b340a7` / PR #6
> 预计阅读时间: 10-15 分钟 | 面向角色: A

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：让 HDMI 启动不再依赖已经停用的 DSI

* **改动背景与问题**：旧链路虽然只使用 HDMI，却仍让全局复位和 `pixel_data_en` 等待 DSI PLL/面板状态。像一栋楼已经封掉旧电梯，却仍把大厅照明接在旧电梯的“就绪”触点上；电梯不启动，整栋楼也不开灯。
* **源码剖析**：顶层取消 `arst_n` 对 DSI PLL lock 的依赖，并在 `i_sysclk_div2` 域使用两级同步释放和本地计数器重新产生像素放行信号，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L684-L701)。DSI 默认处于未定义宏分支，物理输出被拉到不活动状态，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1354-L1478)。
* **经验总结**：删除功能不能只删数据通路，还要追查它是否暗中承担复位、启动或“ready”真源。正确做法是先把仍需要的启动语义迁到本地时钟域，再隔离废弃模块。

### 📌 核心点二：CDC 既要有结构，也要有与真实时钟名一致的约束

* **改动背景与问题**：`reset_pixel_n` 从像素域直接控制 `mipi_clk` 域计数器，曾形成真实负 slack。CDC 就像“隔洋通话”：双方时钟不同，不能拿另一时区的门铃直接当本地复位释放通知。
* **源码剖析**：`soft_mipi_rx_top` 为每个 `mipi_clk` 域增加本地同步释放复位，见 [soft_mipi_rx_top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/mipi_csi/soft_mipi_rx_top.v#L80-L118) 与 [soft_mipi_rx_top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/mipi_csi/soft_mipi_rx_top.v#L215-L217)。SDC 使用 STA 报告中的真实 `mipi_rx_ck*_CLKOUT` 名称建立异步时钟组，见 [constrain.sdc](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/constrain.sdc#L65-L69)。
* **经验总结**：同步器解决电路结构，SDC 解决工具如何分析；两者缺一不可。旧别名或“看起来差不多”的时钟名不会自动约束真实路径。

### 📌 核心点三：显示改善与识别真源必须分开

* **改动背景与问题**：单摄画面经历暗场、绿色偏置与水平重影。候选在 HDMI 支路加入可关闭的固定点白平衡，并让 2:1 像素选择器在每行消隐期重新锚定相位。
* **源码剖析**：固定点增益使用移位加法和饱和裁剪，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1327-L1349)；行相位重新锚定见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1567-L1580)。
* **经验总结**：这像给监视器调色，而不是篡改检测仪原始读数。显示支路可以更易观察，但 CPU 分类和 FPGA 特征仍应基于明确、稳定的识别真源。

> **风险标注：代码审查通过、运行时未触发。** 较早版本曾取得 WNS `+1.674 ns`、WHS `+0.026 ns`，但最终 tip 又加入 HDMI 行相位与固定点白平衡；最终 tip 尚未重跑 Map/PNR/bitstream 和板级五色验证，不能沿用旧报告宣称闭环。

## 2. 可迁移的系统级工程经验

* **避坑警示**：不要因为 CDC 报告没有 synchronizer warning 就跳过 STA；也不要用较早 bitstream 的板级现象替最终源码背书。
* **通用方法论**：按“真实时钟名 → 本地同步结构 → SDC 关系 → 新 PNR → 对应 bitstream → 板级画面”建立单向证据链，每次 RTL/SDC 改动都会使下游证据失效。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：Efinity 时序分析、Clock Relationship Summary、CDC 报告以及 MIPI CSI/DSI IP 生成资料。
  * **应用与意义**：学会从报告中的实际 launch/capture clock 名称反推约束，而不是复制历史 SDC 名称。

* **拓展基础知识**：
  * **推荐学习内容**：异步复位同步释放、双触发器同步器、异步 FIFO Gray 指针和多周期/异步时钟组的适用边界。
  * **应用与意义**：帮助区分“真实 CDC 结构”与“仅用约束隐藏路径”，避免把亚稳态风险包装成时序通过。
