# 视觉模块 Gemini 实施计划审核记录 2026-07-03

> 审核：Claude (Opus 4.8)
> 被审对象：Gemini 起草的《视觉模块架构与寄存器配置实现计划》（APB3 动态寄存器配置 + 硬件 OSD）
> 依据：
> - [vision_module_decision_log_2026-07-02.md](../architecture/vision_module_decision_log_2026-07-02.md)
> - [gemini_review_2026-07-02.md](gemini_review_2026-07-02.md)
> - [riscv_official_examples_integration.md](../architecture/riscv_official_examples_integration.md)
> - [decisions.md](../architecture/decisions.md)（ADR-001 / ADR-002）
> - 项目根 [CLAUDE.md](../../../CLAUDE.md) 的 Codex Gate 规则
>
> 状态：第二轮审核完成。**N1/N2 已退回 Gemini 修改，尚未收回；未生成 Codex Review Packet。**

---

## 0. 总体判断

Gemini 计划的总体方向（APB3 作为 CPU↔FPGA 小特征寄存器主接口、CPU 轮询 `frame_done`、硬件 OSD 叠加 BBox/中心点）与项目已定架构一致，官方 `EfxApb3Example` 例程证明路线可行。但初版有 9 处与项目硬约束冲突或遗漏，第一轮已退回 Gemini 修改；第二轮新版又引入 4 个新问题，N1/N2 需再次退回。

**当前不可直接送 Codex**：N2（OSD 数据源歧义）会阻断 Codex 复核，必须先澄清。

---

## 1. 第一轮修改项落实情况（9 项）

| 项 | 状态 | 说明 |
|---|---|---|
| G1 删除硬编码地址 `0xF000_0000` | ✅ 已改 | 2.2 节改为以 `soc.h` 的 `IO_APB_SLAVE_x_BASE` 为准，并显式提示与 `0xF8100000u` 冲突 |
| G2 APB3 Slave 实例号假设 | ✅ 已改 | 改为实例号待定，注明示例工程中 Slave 0 可能被 gDMA 占用 |
| G3 读清零 → valid/ack 握手 | ✅ 已改 | `SYS_STATUS` 改为 `feature_valid` / `feature_ack` / `frame_id` |
| G4 双像素流 even/odd 对齐 | ✅ 已改 | 3.2 节明确 even/odd 两路比较 |
| G5 OSD 跨时钟域影子寄存器 | ✅ 已改 | 3.1 节加入 VSYNC 边界 shadow register |
| G6 颜色阈值不用 HSV | ✅ 已改 | 3.3 节明确通道差值/比值 |
| G7 Register Map 补齐 | ⚠️ 部分补齐 | 仍有遗漏，见 N3/N4 |
| G8 UART0/UART1 隔离 | ✅ 已改 | 2.3 节明确 UART0(调试/参数) vs UART1(myCobot 1M) 隔离 |
| G9 命名 QCRV32 | ✅ 已改 | 改为 QCRV32（基于 Efinix Sapphire SoC），去掉 v7.3 |
| G10 Codex Gate 状态标注 | ✅ 已改 | 顶部标注"待 Codex 复核" |

第一轮 9 项中 8 项完全落实，G7 做了大部分但仍有遗漏。

---

## 2. 第二轮新问题（4 项）

### 【P1 必改 — 阻断性】

#### N1. CDC 范围必须扩大到 `feature_valid` 与 `frame_id`
- **位置**：Gemini 新版第 3.1 节。
- **问题**：3.1 节只说"一次性将 APB 时钟域写入的全部 BBox 与 Center 坐标锁存到像素时钟域"。但 `SYS_STATUS` 里的 `feature_valid`（单 bit）和 `frame_id`（多 bit）同样跨域：
  - `feature_valid` 单 bit 直接采样会有亚稳态，至少需两级同步器；
  - `frame_id` 多 bit 直接同步会出现"帧号撕裂"（高位和低位不在同一拍），比 BBox 撕裂更危险。
- **改成**：3.1 节把同步对象从"仅 BBox/Center"扩展为"全部跨域数据"——
  - `feature_valid`：两级同步器；
  - `frame_id` 与 BBox/Center 一样走 VSYNC 边界影子寄存器（整组在同一 vsync 锁存），或采用 Gray 编码；
  - 明确 CPU→FPGA 方向（`feature_ack`、`SYS_CTRL`、阈值、ROI）也要同样处理，不要只画单向。

#### N2. 必须先澄清 OSD 画框数据源（FPGA 实时 vs CPU 回写），再决定 CDC 是否必要
- **位置**：Gemini 新版第 3.1 节与 Register Map 0x0A0–0x0A8。
- **问题**：Register Map 中 `RES_BBOX_MIN/MAX/CENTER`（0x0A0–0x0A8）标注为 `R`（FPGA 写、CPU 读），说明 bbox 由 **FPGA 像素域实时统计**产生。但第 3.1 节描述"BBox/Center 坐标由 CPU 经 APB 时钟域异步写入"，两者语义矛盾。这影响 3.1 整段 CDC 是否成立：
  - 若 OSD 画的是 **FPGA 实时 bbox**：数据本就在像素域，画框不需要 CDC 影子寄存器；3.1 对"画框坐标"的 CDC 论述应删除或改为针对 CPU 回写的稳态 overlay 字段。
  - 若 OSD 画的是 **CPU 多帧滤波后的稳态 bbox**：则需要 CDC + 影子寄存器，但应在 Register Map 增设 CPU→FPGA 的回写 bbox 寄存器（`OSD_BBOX_*`），与 0x0A0 的实时结果分开。
- **改成**：在 3.1 节开头明确"OSD 画面同时叠两类内容"：
  1. **FPGA 实时 bbox**（0x0A0 系列，复用统计结果，无需 CDC）；
  2. **CPU 回写的稳态结果**（`OSD_WRITEBACK_A` 扩展为含稳态 bbox/分类，这类才需要 CDC 影子寄存器）。
  并据此调整 Register Map：保留实时 bbox 只读；新增/明确 CPU 回写的 OSD overlay 寄存器区。

### 【P2 建议一并改，不阻断 Codex】

#### N3. 颜色阈值与面积寄存器要覆盖三色或显式标注预留
- **位置**：Register Map。
- **问题**：阈值只列了 `0x020 CFG_COLOR_RED_TH`，缺 BLUE/YELLOW 的阈值偏移；面积集与 [decision_log 第 5 节](../architecture/vision_module_decision_log_2026-07-02.md#L229-L237)（red/blue/yellow/white/black）不一致。
- **改成**：
  - 阈值按 `0x020 RED / 0x024 BLUE / 0x028 YELLOW` 连续排布；
  - 面积 `0x080/0x084/0x088`，并标注"首版仅实现 RED/BLUE/YELLOW，WHITE/BLACK 预留偏移 `0x08C/0x090`，首版可不实现"。

#### N4. `CPU_HEARTBEAT` 拆成独立寄存器或明确位语义
- **位置**：`0x004 SYS_CTRL[8]`。
- **问题**：心跳需 CPU 周期性累加，单 bit 无法承载；"喂狗"语义不清（脉冲还是电平）。
- **改成**（二选一）：
  - 拆成独立 `0x00C CPU_HEARTBEAT`（32-bit 计数，CPU 写递增值，FPGA 看门狗比对超时）；
  - 或保留 `SYS_CTRL[8]` 但明确"CPU 写 1 产生一拍脉冲，FPGA 看门狗在该脉冲沿清零超时计数"，标注为脉冲型而非电平型。

---

## 3. 推进状态

- 第一轮 9 项已退回 Gemini 并完成修改（G7 部分）。
- 第二轮 N1/N2 已再次退回 Gemini 修改，N3/N4 建议 Gemini 一并改。
- 第三轮核对：N1–N4 全部落实（见第 4 节），计划已满足送 Codex 前置条件。
- 已生成 Codex Review Packet：`codex_review_packet_vision_module_2026-07-03.md`，待 Codex 复核。
- CLAUDE.md 的 Codex Gate 已被本计划命中（SoC IP 配置变更、OSD 回写、QCRV32/CPU 寄存器接口、视频时序/CDC）。在 Codex 复核通过前不得更新 decision_log 或起草最终《寄存器手册》。

---

## 4. 第三轮核对（v3，[vision_module_implementation_plan.md](../architecture/vision_module_implementation_plan.md)）

| 项 | 应改 | 实际落实 |
|---|---|---|
| N1 CDC 扩到全部跨域数据 | valid/frame_id/阈值/ROI/OSD 回写都要同步 | ✅ 3.1 节明确所有 CPU 下发的多 Bit 寄存器整组 VSYNC 锁存；`feature_valid`/`feature_ack` 单 bit 走两级同步器 |
| N2 OSD 数据源歧义 | 区分 FPGA 实时（无 CDC）vs CPU 回写（需 CDC），Register Map 拆开 | ✅ 3.2 节两类框（绿色实时无 CDC / 红色稳态走影子寄存器）；新增 `OSD_BBOX_MIN/MAX`（0x044/0x048），与实时 `RES_BBOX_*`（0x0A0 系列）分开 |
| N3 颜色阈值/面积覆盖 | 阈值补 BLUE/YELLOW，面积标注 white/black 预留 | ✅ 阈值 0x020/0x024/0x028 三色；面积 0x080/0x084/0x088 + white/black 预留 0x08C/0x090 标注 |
| N4 CPU_HEARTBEAT 语义 | 拆独立 32-bit 寄存器 | ✅ 新增 `0x00C CPU_HEARTBEAT` 32-bit 递增计数器，FPGA 看门狗比对；`SYS_CTRL[8]` 已移除 |

结论：N1–N4 全部落实，第一轮 G1–G10 仍保持。计划可送 Codex 复核。

---

## 5. v4 核对与 v4.1 Claude 直改

### 5.1 v4 核对（Codex 第三轮前置条件）

B1/B2/B3/R1–R4 大部分落实，但发现 5 项残留：

| 项 | 性质 | 说明 |
|---|---|---|
| P4 | 实质技术漏洞 | `SYS_ACK` 只写"脉冲清 valid"，未承载 frame_id，R1"frame_id 匹配 ack"事件协议无法落地，退化为脉冲清零仍有撕裂 |
| P5 | 文档不自洽 | 3.3 节双像素流仍写 `wb0_data_out`，与 B2 主通道 wb1 矛盾 |
| P6 | B1 完整性 | 第 0 节只有校验机制，缺待导出 soc.h 字段清单 |
| P7 | B2 完整性 | 3.2 节只留"需做 fanout 验证"口头要求，未把已核实事实落表 |
| P8 | R3 细节 | 3.4 节提到 `TH_LUMA`，Register Map 无亮度门限寄存器 |

### 5.2 v4.1 Claude 直改（[vision_module_implementation_plan.md](../architecture/vision_module_implementation_plan.md)）

用户授权 Claude 直接修改并注明修正记录。已落 5 改：

- **P4**：`SYS_ACK`（0x00C, W）描述改为"写入值 [15:0]= 要确认的 frame_id；FPGA 仅在同步后 ack 且 `ack.frame_id == 当前快照 frame_id` 匹配时才清 `feature_valid`"。
- **P5**：3.3 节风险句 `wb0_data_out[47:0]` → `wb1_data_out[47:0]`。
- **P6**：第 0 节新增第 3 条"待导出字段清单"（UART0/UART1、CLINT、PLIC、APB user window、AXI user window、DDR base/size、`SYSTEM_CLINT_HZ`、空闲 `io_apbSlave_x`、PADDR 宽度、PSLVERROR 语义，附录写入寄存器手册）。
- **P7**：3.2 节插入 fanout 事实表（wb0 旁路未接显示、wb1→DSI[top.v:1398-1401]/HDMI[1468-1474]、OSD/feature_extract 拟加在 wb1 之后）。
- **P8**：Register Map 新增 `0x03C CFG_LUMA_TH`（[31:16] TH_LUMA_MAX / [15:0] TH_LUMA_MIN），各色 mask 统一引用。

修订记录已在计划文档顶部 v4.1 条标注。

### 5.3 推进状态

- v4.1 计划满足 Codex 二次复核前置条件。
- Codex 二次复核结论：**不解除 RTL/SoC 执行前 Gate**；B2/B3/R1/R3/R4 已收敛，新增两项执行前阻塞：**B4**（多寄存器配置缺原子提交协议）、**B5**（B1 仅文档收敛、实物 soc.h 未生成）。允许起草寄存器手册草案，状态写"架构方向通过，RTL/SoC 前置待 B4/B1 闭合"。"零改顶层"只对选 wb1 主通道成立，插 OSD 时仍需单独 top.v 连线 Gate。
- Claude 已按 B4 直改 plan 至 v4.2（见 §6）。B5 为状态表述，已写入 Packet 对照表。

---

## 6. v4.2 Claude 直改（按 Codex 二次复核 B4）

Codex 二次复核新增 B4（多寄存器配置缺原子提交协议）与 R1 补充建议（掉帧行为二选一）。Claude 直接修改 [vision_module_implementation_plan.md](../architecture/vision_module_implementation_plan.md) 至 v4.2：

- **B4 原子提交协议**：新增 §3.5"staging → commit → active shadow"三段机制——APB 域 staging/active 双套配置，CPU 写完整组后写 `CFG_COMMIT.cfg_seq` 才触发，VSYNC 边界整组原子切换；Register Map 新增 `0x04C CFG_COMMIT`(W) / `0x050 CFG_STATUS`(R, active_seq/pending_seq)。验收：未 commit 前沿用上一组，不半帧/半组生效。
- **R1 掉帧行为写死**：§3.1 补"valid 未被匹配 ack 前不覆盖快照；CPU 只 ack 当前帧号；掉帧时 FPGA 采用'保持最新快照等 ack'策略"，避免实现者猜测。
- **B5 状态表述**：plan 修订记录 v4.2 条标注"架构方向通过，RTL/SoC 执行前 Gate 未解除（待 B1 soc.h 摘要生成）"。

### 6.1 推进状态

- v4.2 plan + Codex 二次复核结论指向：**起草寄存器手册草案可启动**，状态写"架构方向通过，RTL/SoC 前置待 B4(已闭环)/B1(soc.h 摘要待生成) 闭合"。
- 解除"RTL/SoC 执行前 Gate"的最终前置：补 B1 的 `generated_soc_summary_YYYY-MM-DD.md`（UART/CLINT/PLIC/APB/AXI/DDR/时钟/Slave 摘要）后再最终解除。
- Codex 三次复核（手册 + Gate）前，不进 SoC/IP 配置、不改 top.v、不改 CPU BSP。

---

## 7. v4.3 Claude 直改 + Codex 三次复核结论

### 7.1 Codex 三次复核

- 结果文件：[`codex_review_result_vision_module_round3_2026-07-03.md`](codex_review_result_vision_module_round3_2026-07-03.md)
- **B4 闭环**：`CFG_COMMIT`/`CFG_STATUS` + staging→commit→active shadow 可作为寄存器手册契约基线。
- **B5 表述准确**："架构方向 Gate 解除；RTL/SoC 执行前 Gate 保留，待 `generated_soc_summary` 闭合"。
- 非阻塞建议：手册补 4 个边界（config_seq 复用/回绕、pending 覆盖规则、复位默认值、staging 读回语义）。
- 文档小问题：Packet 标题原写"v4.1 二次复核"与正文 v4.2 三次不符——已顺手改掉。

### 7.2 v4.3 Claude 直改

按 Codex 三次复核的 4 个边界，Claude 直接修改 [`vision_module_implementation_plan.md`](../architecture/vision_module_implementation_plan.md) 至 v4.3，在 §3.5 补：

1. `config_seq` 不可连续两次相同（否则 FPGA 视为无效 commit）；
2. pending 覆盖"最后一次 commit 胜出"；
3. 复位默认 `active_seq=0/pending_seq=0` + 安全默认 ROI/阈值/OSD关闭；
4. 读 `CFG_*` 得 staging、读 `CFG_STATUS.active_seq` 判断是否生效。

### 7.3 当前状态（最终）

- ✅ **架构方向 Gate 已解除**（Codex 三次确认）。
- 🔶 **RTL/SoC 执行前 Gate 保留**，待 B1 `generated_soc_summary_YYYY-MM-DD.md` 闭合（含 UART/CLINT/PLIC/APB/AXI/DDR/时钟/空闲 `io_apbSlave_x`/PADDR/PSLVERROR，及 BSP 占位地址被 soc.h 真源替代）。
- ⚠ OSD 真正插入 `top.v` 前另交小包审查 fanout/timing（"零改顶层"只对选 wb1 主通道成立）。
- **可起草《FPGA 与 CPU 视觉交互寄存器手册》草案**，状态标"架构方向通过，RTL/SoC 前置待 B1 闭合"。