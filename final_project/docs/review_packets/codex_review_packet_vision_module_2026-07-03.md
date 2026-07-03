# Codex Review Packet — 视觉模块架构与寄存器配置实现计划（v4.2 三次复核）

- 日期：2026-07-03
- 模式：Mode A 架构初设 → Codex 三次复核（手册草案 + RTL 执行前最终 Gate）
- 计划来源：[`final_project/docs/architecture/vision_module_implementation_plan.md`](../architecture/vision_module_implementation_plan.md)（**v4.2**）
- 第一轮 Codex 结果：[`codex_review_result_vision_module_2026-07-03.md`](codex_review_result_vision_module_2026-07-03.md)（方向通过 + B1/B2/B3 + R1–R4）
- 第二轮 Codex 结果：[`codex_review_result_vision_module_round2_2026-07-03.md`](codex_review_result_vision_module_round2_2026-07-03.md)（B2/B3/R1/R3/R4 收敛；新增 **B4 原子提交**阻塞、**B5 SoC 实物前置未满足**）
- 审核记录：[`gemini_plan_review_2026-07-03.md`](gemini_plan_review_2026-07-03.md)（含 v4/v4.1/v4.2 核对）
- 状态：**本轮为方案级审查，未改动任何 RTL / 约束 / 工程文件。架构方向 Gate 已解除；RTL/SoC 执行前 Gate 仍未解除（待 B1 soc.h 摘要生成）。**

---

## 这次复核与上次的区别

第二轮 Codex 判定 v4.1 已收敛 B2/B3/R1/R3/R4，但新增两项执行前阻塞：
- **B4**（多寄存器配置缺原子提交协议）——我 Packet 第 5 问提的 CDC 多 bit 写原子性风险，Codex 确认成立。
- **B5**（B1 仅文档收敛，当前无正式 `soc.h` 快照，`bsp.h` 仍是占位）。

Claude 已按 B4 直改 plan 至 v4.2。本次提交请 Codex 判断 B4 是否闭环、手册草案是否可作为契约基线。

### v4.2 相对第二轮的修复对照

| 第二轮 Codex 项 | 修复 | 落点 |
|---|---|---|
| **B4** 配置缺原子提交协议 | ✅ 新增 §3.5"staging→commit→active shadow"三段机制；Register Map 新增 `0x04C CFG_COMMIT`(W, 写 [15:0] config_seq) / `0x050 CFG_STATUS`(R, active_seq/pending_seq)；未 commit 前沿用上一组完整配置，VSYNC 边界整组原子切换 | plan §3.5, §4 |
| **R1 补充** 掉帧行为二选一未写死 | ✅ §3.1 补"valid 未被匹配 ack 前不覆盖快照；CPU 只 ack 当前 `SYS_STATUS.frame_id`；掉帧时 FPGA 采用'保持最新快照等 ack'策略" | plan §3.1 |
| **B5** 状态表述 | ✅ plan 顶部 v4.2 修订记录与 Packet 状态均写"架构方向通过，RTL/SoC 执行前 Gate 未解除（待 B1 soc.h 摘要生成）"；B1 在对照表标"文档约束已收敛, 实物摘要待生成" | 本节对照表 |
| 「零改顶层」语义澄清 | ✅ 注明"零改顶层"仅对选 `wb1_*` 主通道成立；真插 OSD 仍会改 `top.v` 连线，需单独 Gate | 本节"待澄清" |

### B1 状态订正（按 B5）

| 项 | 状态 |
|---|---|
| B1 文档约束（soc.h 字段清单、`#ifndef` 编译期、`REG_MAGIC` 运行期） | ✅ 已收敛 |
| B1 实物前置（当前工程生成的 `soc.h` 摘要） | ❌ 未满足，`final_project` 下无正式快照，[bsp.h:43](../../cpu/app/include/bsp.h#L43) 仍占位 `0xF8100000u` |
| 解除"RTL/SoC 执行前 Gate"前置 | 需补 `generated_soc_summary_YYYY-MM-DD.md`（UART/CLINT/PLIC/APB/AXI/DDR/时钟/空闲 `io_apbSlave_x`/PADDR/PSLVERROR）后再最终解除 |

---

## 任务目标

定义 FPGA（视觉前端）与板上 QCRV32 CPU 之间的第一版软硬件接口与调试链路：

1. 用 APB3 Slave 承载配置寄存器下发与识别结果回读（小特征量，几十字节级）。
2. CPU 采用 `valid`/`ack` + `frame_id` 匹配的轮询握手（非中断、非读清零、非脉冲误清）。
3. 颜色识别用 FPGA 通道差值/比值双边界 + 亮度门限 + 像素级 mask 面积统计，CPU 做多帧滤波与最终分类。
4. 形状识别 FPGA 只输出 bbox/面积原始几何特征，CPU 算长宽比/占空比做最终分类。
5. 硬件 OSD 在 HDMI（`wb1_*` 主显示链路）上叠加 FPGA 实时 bbox（绿）与 CPU 回写稳态 bbox（红）+ 中心十字。
6. 动态参数通过 UART0(115200) ASCII 命令下发；myCobot 走独立 UART1(1M)，物理隔离。
7. `CPU_HEARTBEAT` 看门狗超时仅重置视觉状态机，**禁止触发 myCobot 急停/动作**。

本计划**不**在本轮落地 RTL；二次复核通过后起草《FPGA 与 CPU 视觉交互寄存器手册》并更新 decision_log。

## 当前结论

- 总体路线与项目已定架构一致（[ADR-001](../architecture/decisions.md)、[ADR-002](../architecture/decisions.md)、[vision_module_decision_log_2026-07-02.md](../architecture/vision_module_decision_log_2026-07-02.md)、[gemini_review_2026-07-02.md](gemini_review_2026-07-02.md)、[riscv_official_examples_integration.md](../architecture/riscv_official_examples_integration.md)）。
- 第一轮 Codex 已判定方向通过；v4.1 已补齐全部阻塞前置条件 B1/B2/B3 与主风险 R1/R3/R4，R2 转为 RTL 前置。
- 官方 `EfxApb3Example` 例程存在（`赛方提供材料/例程/RISC-V例程/01_eth_test-v4/ip/gDMA/T120F576_devkit/embedded_sw/soc_dma_exp_0/software/standalone/EfxApb3Example/src/main.c`），证明 APB3 路线可行。
- Codex 第一轮 B2（wb1 主通道）已由 Claude 核实成立：[top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401) DSI、[1468-1474](../../fpga/rtl/top/top.v#L1468-L1474) HDMI 均消费 `wb1_*`；`wb0_*` 定义但未接显示；赛方原工程同位置同信号，PR1 照迁未改动。

## 修改或计划涉及的文件

**本轮方案级，未改动 RTL/约束/工程文件。** 后续执行将涉及（待 Codex 二次通过后启动）：

- `final_project/fpga/rtl/cpu_if/`：APB3 target 状态机 + 寄存器文件（新建）。
- `final_project/fpga/rtl/osd/`：OSD 叠加模块 + VSYNC 影子寄存器（新建）。
- `final_project/fpga/rtl/feature_extract/`：颜色 mask / area / bbox 统计（新建，取 `wb1_*`）。
- Efinity SoC IP 配置：使能一个空闲 APB3 Slave 实例 + 双 UART（编号待 SoC 配置锁定）。
- `final_project/cpu/app/include/bsp.h`：基地址宏改为以生成 `soc.h` 为准，配 `#ifndef` 编译期报错。
- 不动 `ipm/`、赛方补丁、原始压缩包、波形、`outflow/`；本轮确认**不动 `top.v` 顶层视频连接**（B2-a 零改顶层）。

## 关键模块与信号链路

```text
wb1_data_out[47:0] (白平衡后, 双像素 RGB888, 像素时钟域, = 主显示链路)
 -> feature_extract: ROI 裁剪 -> 颜色 mask(双边界阈值+亮度门限) -> area 统计
                  -> bbox(MIN_X/MAX_X/MIN_Y/MAX_Y) + center
 -> 寄存器文件 (像素域写, R 类: LIVE_*_AREA / LIVE_BBOX_* / LIVE_CENTER / frame_id / feature_valid)
 -> (同一路 wb1_*) OSD 叠加 -> DSI/HDMI

wb0_* (旁路, 首版未用, 预留后续侧视相机)

APB3 Slave (APB 时钟域, QCRV32 侧)
 -> 写: SYS_CTRL / SYS_ACK(含 frame_id) / CPU_HEARTBEAT / CFG_ROI_* / CFG_*_TH_0/1 / CFG_LUMA_TH / CPU_OSD_CTRL / CPU_OSD_BBOX_*
 -> 读: REG_MAGIC / SYS_STATUS(valid+frame_id) / LIVE_*_AREA / LIVE_BBOX_* / LIVE_CENTER

OSD (像素时钟域)
 <- 实时 bbox (像素域 LIVE_*, 无 CDC, 绿色框)
 <- CPU 回写稳态 bbox (APB域 CPU_OSD_BBOX_* -> VSYNC影子寄存器 -> 像素域, 红色框)
 -> 双像素 even/odd 两路坐标比较, 覆盖边框/中心十字
 -> 接在 wb1_white_balance 之后、DSI TX / HDMI 串化之前
```

## 时钟、复位、AXI、framebuffer、双通道影响

- **时钟域**：APB 时钟域（QCRV32 SoC，频率待 `soc.h`，参考 `SYSTEM_CLINT_HZ`）与视频像素时钟域（白平衡输出 `wb1_*`）。特征量与 OSD 天然在像素域，APB3 在 SoC 域。
- **CDC 处理（已升级为 R1 事件协议）**：
  - 多 bit 跨域（`SYS_CTRL`、阈值、ROI、`CPU_OSD_BBOX_*`）：VSYNC 边界影子寄存器整组锁存，帧内配置绝对一致。
  - `frame_id`：天然由像素域产生，随 `RES_*` 在帧末整组快照到 APB 读侧；valid 期间数据保持稳定，ack 帧号匹配后更新。不走 Gray 编码作首选。
  - `feature_valid`（FPGA→CPU 单 bit）：两级同步器。
  - `feature_ack`（CPU→FPGA）：CPU 写 `SYS_ACK[15:0] = 要确认的 frame_id`；FPGA 仅在同步后 ack 且 `ack.frame_id == 当前快照 frame_id` 匹配时清 `feature_valid`，**避免任意脉冲误清**。
- **复位**：APB3 复位由 SoC 提供；统计/OSD 复位沿用视频链路 `pixel_data_en`（PR1 已确认其由 DSI 例化产生、被 AXI/debayer/white_balance/HDMI/framebuffer 多处当 rst/enable 消费，不可裸删；OSD 插入**不改变**其产生链路）。
- **AXI / framebuffer**：本计划不触碰 AXI 互连与 framebuffer 地址；特征量走寄存器不经 DDR，符合 gemini_review 建议1（规避 DDR 带宽抢占与 cache 一致性）。
- **双通道**：第一版只用 `wb1_*`（主相机俯视）；`wb0_*` 旁路预留后续侧视相机。本轮不在 RTL 落地。
- **双像素流**：`wb1_data_out[47:0]` 为 even+odd 双像素，OSD 与 bbox 统计必须按双像素步进，否则半像素错位。

## 已运行验证

- 命令：无（本轮为方案级审查，未改 RTL、未综合、未仿真）。
- 静态核对项：
  - QCRV32 / Sapphire 命名核对：[bsp.h:2](../../cpu/app/include/bsp.h#L2)、[startup_qcrv32.S:2](../../cpu/app/startup/startup_qcrv32.S#L2)、README.md。
  - APB3 例程存在性核对：见"当前结论"。
  - APB3 Slave 占用核对：`赛方提供材料/例程/RISC-V例程/03_SD_test/sdhc/Ti375C529_devkit/source/top_soc.v:305-672` 显示 `io_apbSlave_0` 在示例中被 gDMA 占用。
  - 地址两套风格核对：[riscv_official_examples_integration.md:30-40](../architecture/riscv_official_examples_integration.md#L30-L40)。
  - **B2 主通道核实（Claude 已对工程与赛方原样双查）**：[top.v:1291-1310](../../fpga/rtl/top/top.v#L1291-L1310) `wb0_*` 定义未接显示；[top.v:1398-1401](../../fpga/rtl/top/top.v#L1398-L1401) DSI 取 `wb1_*`；[top.v:1468-1474](../../fpga/rtl/top/top.v#L1468-L1474) HDMI 串化取 `wb1_data_out`；赛方原工程同行号同信号。
- 关键 warning：无综合 / 仿真输出（方案级）。

## 机械臂 / 外设状态

- myCobot 是否涉及：**否（本计划不涉及机械臂动作）**。仅约定 UART1(1M) 预留隔离。
- COM 口 / CP210x 状态：未涉及。
- 波特率：UART0=115200（调试/参数下发）；UART1=1000000（myCobot 预留，正式启用前需按 myCobot 资料二次确认并走 myCobot 280 联调流程）。
- 是否执行动作：否。
- 安全确认：本计划不触发机械臂任何动作；`CPU_HEARTBEAT` 看门狗超时仅重置视觉状态机，**明确禁止触发 myCobot 急停/动作**，安全语义已隔离。

## 未验证项和风险假设

1. **APB3 Slave 实例号未锁定**：示例工程 Slave 0 被 gDMA 占用，正式工程须先确认空闲实例；若全部被占则需在 SoC 配置里腾出，可能影响 `soc.h` 与 link 脚本。B1 已加双重校验缓解，但地址摘要在 RTL 启动前必须导出。
2. **基地址未锁定**：`IO_APB_SLAVE_x_BASE` 依赖 Efinity 2025.2 生成的 `soc.h`；[bsp.h:43](../../cpu/app/include/bsp.h#L43) 中的 `0xF8100000u` 仅为占位。
3. **APB 时钟频率未锁定**：CDC 同步器深度与 VSYNC 影子寄存器时序窗口依赖实际 APB/像素时钟比；若 APB 远慢于像素时钟，写跨 VSYNC 落到同一帧的假设需在 RTL 阶段验证。
4. **OSD 插入点（R2，转为 RTL 前置）**：插在 `wb1_white_balance` 之后、DSI/HDMI 之前。RTL 启动前必须做 OSD 插入后的 fanout/timing diff，确认不破坏 `pixel_data_en` 复位链与 HDMI 48→24 bit 串化时序/奇偶顺序。
5. **面积阈值/尺寸分类阈值寄存器**：首版 Register Map 不列独立的尺寸分类阈值，尺寸分类由 CPU 查表（`param_table.c`）完成；多帧滤波/迟滞在 CPU。
6. **多物体**：第一版单物体，bbox 只输出"最大物体"；多物体需重构统计单元（decision_log 阶段 4）。
7. **PLIC 中断**：本计划保留轮询，未引入中断；若后续上中断需重走 PLIC/CDC 评估。

## 希望 Codex 判断的问题

1. **B1/B2/B3 是否充分落实**：相对于第一轮，三项阻塞前置条件在 v4.1 是否已收敛到可进入 RTL 的程度？还有无遗漏的 SoC/地址/握手层硬约束？
2. **R1 事件协议是否真正落地（重点核 P4）**：`SYS_ACK` 承载 [15:0] frame_id + FPGA 仅帧号匹配才清 valid 的方案，是否完整闭合"稳定快照 + frame_id 匹配 ack"？是否存在 ack 同步、帧号回绕、多帧排队等边界问题？`feature_valid` 仍用两级同步器配合该协议是否够稳，还是需要升级到握手指令化（valid-only-then-stable）？
3. **B2-a 零改顶层是否成立**：第一版主相机/OSD/feature_extract 全部取 `wb1_*`、不动 `top.v`、`wb0_*` 旁路预留——在该约束下 OSD 插入点与现 HDMI 串化（[top.v:1468-1474](../../fpga/rtl/top/top.v#L1468-L1474)）是否会有时序/扇出冲突？R2 的 fanout 验证是否足以覆盖？
4. **Register Map 是否可以作为《寄存器手册》契约基线**：现状命名 `LIVE_*` / `CPU_OSD_*`、STATUS/ACK 拆分、双边界阈值 + `CFG_LUMA_TH`、心跳独立寄存器等，是否有仍会造成 CPU 误用或 FPGA 实现歧义的字段？偏移排布（0x000–0x0A8）是否有地址对齐或扩展预留隐患？
5. **CDC 多 bit 写跨域**：CPU 写阈值/ROI/`CPU_OSD_BBOX_*` 经 VSYNC 整组锁存，若 CPU 在一帧内分多次写完一组参数（如 ROI 两个字分两次写），是否存在"半新半旧"参数被同一 VSYNC 锁存的风险？是否需要在协议层加"写完整组后再让 FPGA 取数"的原子性约定？
6. **颜色阈值鲁棒性**：通道差值双边界 + 亮度门限 + 比值方案，对现场 AWB/曝光波动的鲁棒性是否足够？是否还需保留额外可配置余量（如每色独立 luma 门限而非全局 `CFG_LUMA_TH`）？
7. **心跳看门狗语义**：`CPU_HEARTBEAT` 32-bit 递增 + FPGA 超时仅重置视觉状态机、不动 myCobot——超时阈值量级（毫秒 vs 帧数）与重置范围是否需要在本计划内进一步钉死，还是留到 RTL 阶段？
8. **是否可以解除"RTL/SoC 执行前 Codex Gate"**：通过后即起草《寄存器手册》并更新 decision_log，是否还有遗漏的执行前置条件？