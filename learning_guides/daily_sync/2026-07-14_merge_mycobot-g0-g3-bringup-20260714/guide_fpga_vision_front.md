# 学习指南：FPGA与视频前端 - 隔离构建边界、PNR 阻塞与 APB 寄存器对接
> 生成日期: 2026-07-14 | 对应分支合并: codex/mycobot-g0-g3-bringup-20260714@d433bca
> 预计阅读时间: 10-15 分钟 | 面向角色: A (FPGA与视频前端)

---

## 1. 今日最核心的改动与经验

### 📌 核心点一: PNR 约束未对齐导致的 outpad 断言报错
* **改动背景与问题**：
  在本次集成测试中，两位队友的 CPU 分支合并后，FPGA 工程进行了 `map`（综合）和 `PNR`（布局布线）的完整性构建。虽然 `map` 阶段顺利通过（debug map 资源占用：`EFX_ADD=1827`、`EFX_LUT4=10339` 等），但是 PNR 阶段不幸失败，工具报出大量的 `outpad` 错误，提示有 1776 个 I/O 没有指定具体物理位置。
* **源码与工程剖析**：
  在 [CURRENT_STATE.md (line 44-51)](file:///d:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L44-L51) 及 [mycobot_cpu_board_bringup_implementation_plan_20260714.md (line 107-112)](file:///d:/第十届集创赛-雄芯院材料/final_project/docs/technical_plans/mycobot_cpu_board_bringup_implementation_plan_20260714.md#L107-L112) 中记录，FPGA 顶层 [top.v](file:///d:/第十届集创赛-雄芯院材料/final_project/fpga/rtl/top/top.v) 导出了过多的外设端口（例如 UART2 引脚等），但在 Interface Designer（`.peri.xml`）中并未为这些端口分配具体的物理 PIN 脚或进行正确的引脚约束锁定，导致 PNR 工具无处安放这些端口而抛出断言崩溃。
* **经验总结（大白话比喻）**：
  这就像是**“家具买好了，却发现房子没画门窗”**。我们写Verilog代码的时候声明了大量的外部引脚接口（买回了精美的柜子、大沙发），但是在 Efinity 的外设配置界面里（.peri.xml）却没有给这些端口划定具体的硬件管脚（房子没留出可以放柜子、沙发的门窗位置），导致布局布线工具搬家具的时候直接“卡在门外”，最终报出内部断言错误。下阶段必须先利用 Interface Designer 规范化 periphery I/O 绑定，禁止直接在顶层盲补端口声明。

### 📌 核心点二: SoC 与视频 PLL 资源冲突与重规划决策
* **改动背景与问题**：
  在实现 CPU 与 FPGA 的 APB 通信时，硬核 RISC-V SoC 的系统时钟（`PLL_SOC_SYS_RESOURCE`）只被允许占用 `PLL_BL0/BL1/BL2`，但这三个资源在正式的视频工程中已经完全被 MIPI、DDR 和视频前端所占用，硬件资源发生了正面冲突。
* **工程审计剖析**：
  在 [CURRENT_STATE.md (line 179-185)](file:///d:/第十届集创赛-雄芯院材料/CURRENT_STATE.md#L179-L185) 记录中明确，A4 阶段已封锁了直接合并 SoC 的操作。严禁手动修改 `.peri.xml` 或手改生成的 SoC 内部时钟连线，必须在 Interface Designer 的 GUI 界面中检查 `pll_inst1` 等 Titanium 迁移路径的可行性，确认存在合法候选方案后方可推进。
* **经验总结（大白话比喻）**：
  这相当于**“两只大象抢走同一条窄通道”**。SoC 必须要走主干道 A（PLL_BL），但视频显示和外设也强行占领了通道 A，如果两边都不让步，强行把两套时钟逻辑连在一起（手改 XML 或引脚），在芯片物理布线上就会导致严重的干扰、抖动甚至根本无法工作。在时钟 PLL 资源重新分配并取得 GUI 完美验证前，CPU 与 FPGA 通信的 APB 链路必须保持“隔离基线状态”，以防损害原有的视频前端稳定性。

---

## 2. 可迁移的系统级工程经验

* **避坑警示**：
  在调试 CPU 与 FPGA APB0 总线寄存器交互时，切忌在顶层端口随意拉出 debug 信号或直接拉低未驱动的 `PREADY` / `PSLVERROR` 信号。在 [mycobot_cpu_board_bringup_implementation_plan_20260714.md (line 196-201)](file:///d:/第十届集创赛-雄芯院材料/final_project/docs/technical_plans/mycobot_cpu_board_bringup_implementation_plan_20260714.md#L196-L201) 中揭示，位宽从 32 位到 12 位的隐式截断如果处理不当，会导致地址总线寻址错乱，在综合阶段会被优化掉部分敏感寄存器。
* **通用方法论**：
  **“生产（Production）与调试（Debug）构建完全隔离”**。工程中明确将 debug 用的合成特征源（`COMPETITION_DEBUG_SYNTHETIC=1`）与真实视频源通过条件编译隔离在顶层之外。任何在合成视频源下能够通过的 OSD 渲染、分类器和特征探针，都**“不代表真实物理摄像头上板通过”**，这两种制品应当在 manifest 中有独立的 SHA-256 和构建名称，不可混为一谈。

---

## 3. 推荐自行学习的相关知识

* **优先赛方资料**：
  * **推荐学习内容**：查阅 `赛方提供材料/EDA软件培训文档及视频/` 下关于 Efinity Interface Designer、Titanium 资源规划手册及 PLL 实例化生成说明。
  * **应用与意义**：弄清如何为 RISC-V SoC 系统分配合理的 PLL 资源，并在不破坏 DDR 时钟及视频采集时钟的前提下，让 FPGA 与 SoC APB 总线安全共存，以此解决当前的 PNR 挂起阻塞。

* **拓展基础知识**：
  * **推荐学习内容**：自学 APB（Advanced Peripheral Bus）总线的时序关系，重点在于主从设备间的握手（`PSEL`、`PENABLE`、`PREADY`）以及跨时钟域（CDC）的双拍同步打拍机制。
  * **应用与意义**：未来需要将 FPGA 前端计算好的前景面积（`fg_area`）和 ROI bbox 信息安全地写入 CPU 的 MMIO 空间。掌握 CDC 机制可防止 CPU 在读取寄存器时因为亚稳态导致读入错乱的数据，确保视频结果交互的严密无误。
