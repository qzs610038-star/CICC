# 学习指南：嵌入式 CPU 与通信控制 - 视频候选合并后的接口真源边界
> 生成日期: 2026-07-15 | 对应分支合并: `dev/libaoxun688@8b340a7` / PR #6
> 预计阅读时间: 10-15 分钟 | 面向角色: B

---

## 1. 今日最核心的改动与经验

### 📌 核心点一：这次没有改 CPU，但改变了 CPU 未来接收数据的前端假设

* **改动背景与问题**：本次 8 个文件全部位于隔离单摄候选工程，没有修改 `final_project/cpu`。然而显示支路新增白平衡、传感器 ROM 新增约 2.98 倍模拟增益，意味着“屏幕看起来怎样”和“分类输入是什么”不能再混为一谈。
* **源码剖析**：传感器 ROM 保持数字增益约 1 倍，并增加模拟增益条目，见 [i2c_master_reg_rom.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_reg_rom.v#L242-L249)；发送长度同步改为 165，见 [i2c_master_ctrl_top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/mipi_csi/cam_i2c_ctrl/i2c/i2c_master_ctrl_top.v#L1-L3)。HDMI 固定白平衡只选择显示输出，见 [top.v](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/src/top.v#L1327-L1349)。
* **经验总结**：这像机场安检屏幕可以调亮，但安检判定模型必须知道它读的是原始扫描、传感器增益后数据，还是显示校正后数据。接口契约必须写清采样点。

### 📌 核心点二：CPU 软件通过不等于单摄 SoC 已经接入

* **改动背景与问题**：此前 CPU + myCobot G0-G3 固定提交仍完整保留，但本次 lib 分支没有新增 SoC、APB、OSD wire ABI 或板上事件源。
* **源码剖析**：候选工作日志明确把最终 HDMI 修改记为 `MAP-PNR-BOARD NOT VERIFIED`，见 [WORK_LOG.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L626-L641)。更早的构建只证明复位修复和亮度提升，五色仍未验证，见 [WORK_LOG.md](file:///D:/第十届集创赛-雄芯院材料/competition_project_single_camera/WORK_LOG.md#L604-L623)。
* **经验总结**：CPU Host 测试像“发动机在试验台运转正常”，单摄 FPGA 像“新车身”；两者分别通过不代表线束、仪表和刹车已经接好。接入必须另设 APB/CDC/OSD 契约门。

## 2. 可迁移的系统级工程经验

* **避坑警示**：不要从 HDMI 截图反推 CPU 分类阈值；固定白平衡目前是 display-only，若未来 ROI/统计使用不同采样点，阈值必须按其真实数据重新标定。
* **通用方法论**：为每个跨 FPGA→CPU 字段记录四件事：采样位置、时钟域、数值量纲、有效/更新协议。缺一项就先保持 `UNAVAILABLE` 或阻止执行。

> **风险标注：代码审查通过、运行时未触发。** 本次未实现单摄候选到 CPU 的正式 APB/事件链，现有 CPU G0-G3 仍是 NOT_FOR_FLASH 软件门。

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：QCRV32、APB3、片上存储器和 Efinity SoC 生成流程资料。
  * **应用与意义**：为后续把 ROI/颜色统计从单摄 FPGA 可靠交给板上 CPU 建立正式地址与中断/轮询契约。

* **拓展基础知识**：
  * **推荐学习内容**：生产者-消费者快照、双缓冲寄存器、序号/ACK 与跨域数据一致性。
  * **应用与意义**：避免 CPU 读到一半旧帧、一半新帧，并与现有逐轮 `event_seq` 幂等机制对齐。
