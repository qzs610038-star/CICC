# M0 PNR 前接口归属解决方案

> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，按 Codex2 指令编写 PNR 前修复方案）
> 工程：`competition_project_single_camera/mem_test.xml`
> 范围：仅方案设计。不修改 RTL、SDC、XML、IP。不运行 PNR/STA/CDC/bitstream。
> 结论：**PNR HOLD。四项接口问题均需独立审查，LED 修复与 AXI/JTAG/MIPI 配置变更分开门禁。**

---

## 0. 依据

本方案基于 `m0_efx0256_interface_audit_20260715.md`（2026-07-15 修正版）的静态审计结论，并逐项核查了真实源码：

| 文件 | 核查项 | 结论 |
|---|---|---|
| `top.v:607-608` | led 驱动 | 仅 `led[0]=~ddr_cfg_ok`，`led[1]=vs_cnt[5]`；`led[3:2]` 无驱动 |
| `mem_test.peri.xml:169-173` | led 声明 | `led[3:0]` 四位全部声明为 output |
| `mem_test.peri.xml:393` | JTAG 声明 | 仅有 `jtag_inst1`（JTAG_USER1） |
| `mem_test.peri.xml` (全文件) | jtag_inst2 | **未找到**——无 `jtag_inst2` 条目 |
| `top.v:118-119` | JTAG 端口 | 同时声明 `jtag_inst1_TDO` 和 `jtag_inst2_TDO` |
| `mem_test.peri.xml:455` | axi_target1 | `is_axi_enable="true"` |
| `top.v:1392-1448` | DSI 驱动 | `dsi_tx_top_inst1` 实际驱动 ch1（`mipi_tx_ck1/dp10-dp13`） |
| `top.v:297-331` | DSI ch0 端口 | ch0（`mipi_tx_ck0/dp00-dp03`）端口声明存在但无驱动 |
| `mem_test.peri.xml:875-957` | DSI DPHY | 同时声明 ch0（ck0/dp00-dp03）和 ch1（ck1/dp10-dp13） |

---

## 1. LED（led[3:2]）—— UNEXPECTED

### 1.1 事实

- `top.v:205` 声明 `output [3:0] led`。
- `top.v:607` 驱动 `led[0] = ~ddr_cfg_ok`（D14，DDR 配置完成指示）。
- `top.v:608` 驱动 `led[1] = vs_cnt[5]`（D15，帧计数闪烁）。
- `led[3:2]` 在 RTL 中没有被任何逻辑驱动。
- `mem_test.peri.xml:169-173` 将 `led[3:0]` 四位全部声明为实体 GPIO 输出（`mode="output"`）。

### 1.2 结论

**UNEXPECTED**。不同于 AXI1/DSI 的复杂 IP 端口，LED 是简单 GPIO 输出，不应在综合后仍悬浮。

### 1.3 首选方案

在 `top.v` 中为 `led[3:2]` 赋明确安全常量：

```verilog
assign led[3] = 1'b0;  // 或 1'b1，熄灭/点亮 LED
assign led[2] = 1'b0;
```

**理由**：
- 最小修改，不涉及 periphery、SDC、IP 或任何其他模块。
- 消除 2 个 EFX-0256，在 PNR 前闭合 LED GPIO 族。
- 不影响 `led[1:0]` 的现有功能。

### 1.4 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 硬编码常量可能违反开发板 LED 点亮惯例 | 低 | 可先选 `1'b0`（熄灭），后续按需调整 |
| 不影响视频/HDMI/摄像头 | 无 | LED 不在数据路径中 |

### 1.5 待确认事项

1. 开发板上 `led[3:2]` 对应物理 LED 的亮灭惯例（低电平点亮还是高电平点亮？当前 `led[0]` 为低点亮 DDR 未完成、`led[1]` 闪烁。保险起见赋值 `1'b0` 即保持常灭）。
2. `led[3:2]` 板级引脚是否已引出/可观察（即使不观察，消除 warning 本身也降低 PNR 噪声）。

### 1.6 最小验证顺序

1. 在 `top.v` 追加两行 `assign led[3]` / `assign led[2]`。
2. 重跑 `efx_run --prj -f map`，确认 EFX-0256 从 785 降至 783 且 Map PASS。
3. 确认 `git diff` 仅 LED 两行。
4. 独立审查通过后，方可进入 LED 族的 PNR 验证。

### 1.7 门禁

**LED 修复必须与 AXI/JTAG/MIPI 配置变更分开审查。** LED 是独立 GPIO tie-off，不涉及 Interface Designer、periphery XML 或 IP 重配置。

---

## 2. JTAG（jtag_inst1_TDO / jtag_inst2_TDO）—— TBD

### 2.1 事实

| 项目 | jtag_inst1 | jtag_inst2 |
|---|---|---|
| `mem_test.peri.xml` 声明 | **有**（JTAG_USER1，`GPIOT_PN_11` 域） | **无** |
| `top.v` 输入端口 | 有（TCK/TDI/TMS/TRST 等 10 个） | 有（TCK/TDI/TMS/TRST 等 10 个） |
| `top.v` 输出端口 `TDO` | 有 | 有 |
| Debugger 配置 | 有（`debugger_templates/`） | 无 |
| EFX-0256 | 有 | 有 |

### 2.2 结论

**两者都保持 TBD。** 关键发现：`jtag_inst2` 在 `mem_test.peri.xml` 中根本没有声明，但其顶层端口和 JTAG 输入信号存在于 `top.v`。这说明：

- `jtag_inst2` 可能是 Interface Designer 的历史残留——曾存在后被 GUI 删除，但顶层端口未清理。
- `jtag_inst1` 已正确声明为 `JTAG_USER1` 并有 Debugger 使用，其 TDO 可能由 JTAG 硬核在 PNR 阶段自动连接。

### 2.3 建议

| 项目 | 建议 |
|---|---|
| jtag_inst1_TDO | 用 Interface Designer 或同版本（Efinity 2025.2.288.4.15）历史 PNR 证据确认 JTAG 硬核 TDO 是否在 PNR 阶段由工具自动驱动。如果历史 PNR 中此端口同样有 EFX-0256 但 PNR 仍通过/bitstream 可用，则可标为 EXPECTED（工具自动连接）。 |
| jtag_inst2 | 裁定是否为过期顶层端口。如果是：通过 Interface Designer 删除 `jtag_inst2` 的所有顶层端口声明（不手改 peri.xml），或在 `top.v` 中移除对应端口和输入信号。最小的安全保守方案：先确认 jtag_inst2 在历史 PNR 中是否也存在，存在则暂保留不动。 |

### 2.4 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| 手改 peri.xml 删除 jtag_inst2 | 高 | **禁止**——必须通过 Interface Designer GUI |
| jtag_inst1 TDO 假归因为"工具会自动驱动" | 中 | 需要历史 PNR 证据，不能仅凭推论 |
| jtag_inst2 涉及未发现的 Debugger/工具链依赖 | 低 | 当前 peri.xml 无 jtag_inst2 声明，工具链不应依赖它 |

### 2.5 待确认事项

1. **jtag_inst1_TDO**：用 Efinity 2025.2.288.4.15 运行 Interface Designer，检查 jtag_inst1 的 TDO 连接状态，或查找同一版本工具的历史 PNR 日志确认 TDO warning 为已知工具行为。
2. **jtag_inst2**：确认 `top.v` 中 jtag_inst2 端口的来源——是 Interface Designer 生成了但 peri.xml 未包含，还是手工复制了 jtag_inst1 的端口声明。
3. 如果 jtag_inst2 确为残留：通过 Interface Designer 删除（生成新的 peri.xml），并同步清理 `top.v` 中对应端口。

### 2.6 最小验证顺序

1. 只读打开 Efinity GUI → Interface Designer，检查两个 JTAG 实例的可见性和状态。
2. 如有历史 PNR 日志，复查其中 jtag_inst1/2_TDO 的 warning 状态。
3. 根据 GUI 和日志证据裁定结论，写出单独的 JTAG 决议记录。
4. 不手改 peri.xml。如需修改，必须通过 Interface Designer。

---

## 3. AXI1（698 个未驱动输出）—— TBD / BLOCKING

### 3.1 事实

- `mem_test.peri.xml:455`：`<efxpt:axi_target1 is_axi_width_256="false" is_axi_enable="true">`。
- `axi_target1` 是 DDR IP（`ddr_inst1`）的第二个 AXI 目标端口。
- 所有 698 个 axi1 顶层输出信号（ARADDR[32:0]、AWADDR[32:0]、WDATA[511:0]、WSTRB[63:0] 等）均属于此 AXI target。
- 当前视频工程中，无任何内部逻辑驱动 axi1 的主机端信号。

### 3.2 结论

**TBD / BLOCKING。不能标为 EXPECTED 或无害预留。** `is_axi_enable="true"` 意味着这些端口是 DDR 控制器的**活跃 AXI 目标**，不是闲置预留接口。698 个未驱动输出信号在 PNR 阶段极可能导致 placement 失败或工具断言。

### 3.3 旧错误结论

上一版审计将 axi1 698 个标记为 `EXPECTED — 视频-only 工程无 AXI1 master`。此结论忽略了 `is_axi_enable="true"`——DDR IP 期望有 master 驱动该目标。这不是"当前不用所以无害"的 GPIO 闲置端口。

### 3.4 建议

**方案 A（首选——关闭 axi_target1）**：
通过 Efinity Interface Designer / DDR IP 配置，将 `axi_target1` 的 `is_axi_enable` 改为 `false`，然后重新生成 periphery 和顶层端口。这将从根源消除 698 个未驱动输出——不再出现在顶层、不再产生 EFX-0256。

**方案 B（备选——引入 AXI master）**：
如果未来 SoC/CPU 需要使用 axi1 接口访问 DDR，则为 axi1 接入真实的 AXI master IP。但这不在 M0 范围内。

### 3.5 禁止项

| 操作 | 原因 |
|---|---|
| **手工 tie-off 698 位 axi1 信号** | 不可能——WDATA 512 位、WSTRB 64 位、地址 33 位，手工 tie-off 无意义且不可维护。AXI 协议要求 valid/ready 握手，硬 tie-off 会导致协议死锁。 |
| **手改 mem_test.peri.xml** | 必须通过 Interface Designer 或 DDR IP 配置 GUI，否则生成端口与实际物理引脚不一致。 |
| **盲写 SDC 约束忽略这些端口** | 不能解决未驱动问题，PNR 阶段仍可能触发工具断言。 |

### 3.6 待确认事项

1. 通过 Efinity Interface Designer 打开工程，确认 `ddr_inst1` 的 `axi_target1` 是否可在 GUI 中关闭（`is_axi_enable=false`）。
2. 确认关闭 target1 是否影响现有 framebuffer 的 DDR 访问（`axi_target0` 已足够当前视频工程的双通道帧缓存）。
3. 如果 GUI 不允许单独关闭 target1：确认是否需要重新生成 DDR IP 配置（保留 target0、去除 target1）。

### 3.7 最小验证顺序

1. 只读打开 Efinity GUI → Interface Designer → DDR 配置，检查 axi_target1 的 `is_axi_enable` 设置和可编辑性。
2. 截屏记录当前配置。
3. 确认关闭 target1 不影响 axi_target0 和现有 framebuffer 通道。
4. 如确认可关闭：通过 GUI 关闭 → 重新生成 periphery → 重跑 Map 确认 axi1 EFX-0256 全部消除。
5. 如不可关闭：升级为 BLOCKING → 需 Codex2 裁定是否接受 698 个 warning 进入 PNR 或另寻方案。

---

## 4. MIPI DSI ch0（70 个未驱动输出）—— TBD

### 4.1 事实

- `top.v` 同时声明 ch0（`mipi_tx_ck0`/`dp00-dp03`，lines 297-331）和 ch1（`mipi_tx_ck1`/`dp10-dp13`，lines 332-366）的 DSI TX 输出端口。
- `dsi_tx_top_inst1`（`top.v:1392`）**实际驱动 ch1**——其端口连接全部指向 `mipi_tx_ck1_*`、`mipi_tx_dp10_*` 到 `mipi_tx_dp13_*`。
- ch0（`mipi_tx_ck0_*`、`mipi_tx_dp00_*` 到 `mipi_tx_dp03_*`）在 `top.v` 中**没有任何模块驱动**。
- `mem_test.peri.xml` 同时声明了 ch0（5 lanes）和 ch1（5 lanes）的 MIPI DPHY。
- 70 个 EFX-0256 全部属于 ch0（5 lanes × 14 signals/lane）。

### 4.2 结论

**TBD。** 旧审计描述"整个 DSI 未激活"不准确——ch1 通过 `dsi_tx_top_inst1` 有完整的数据路径驱动（像素流来自 ch0 摄像头经 white_balance 后进入 DSI TX），ch0 才是完全孤立的未驱动通道。

### 4.3 旧错误结论

上一版审计：
- "当前 DSI TX 整体未被激活"——**错误**，ch1 被 `dsi_tx_top_inst1` 驱动。
- "M1 裁剪 ch1/DSI 前，DSI TX 不驱动是预期行为"——**不准确**，ch0 和 ch1 是两个独立 DPHY 通道，ch1 已驱动，ch0 才需要处理。

### 4.4 建议

**方案 A（首选——通过 Interface Designer 关闭 ch0 DPHY）**：
ch0 DPHY 在当前 HDMI-only 工程中不需要，应通过 Interface Designer 移除 ch0 的 5 个 MIPI DPHY lane（ck0/dp00-dp03）和对应顶层端口，并同步 SDC。

**方案 B（备选——如果 GUI 不支持部分移除）**：
保留 ch0 端口但在 `top.v` 中为其赋安全复位值（所有 HS/LP 输出和 OE 信号清零），使 DPHY 输出为安全电气状态。注意：这仅消除 warning，不如方案 A 从根源移除。

### 4.5 注意事项

- **不与 ch1 混淆**：ch1 正在被 `dsi_tx_top_inst1` 驱动，移除 ch0 不应影响 ch1。
- **LCD 控制信号**：`P1_lcd_power_en` 和 `P1_o_lcd_rstn`（另 2 个 EFX-0256）由 `dsi_tx_top_inst1` 通过 `LCD_POWER` / `LCD_RST_P` 端口驱动，但 `P0_lcd_power_en`、`P0_lcd_rstp` 属于 ch0 配套 GPIO。另外还有 `P1_lcd_power_en` 和 `P1_o_lcd_rstn` 在 `top.v:191-194` 声明但 grep 未发现驱动（需进一步确认）。

### 4.6 待确认事项

1. 通过 Efinity Interface Designer 确认 ch0 的 5 个 DPHY lane 是否可以单独移除而不影响 ch1。
2. 确认移除 ch0 后，`P0_lcd_power_en`、`P0_lcd_rstp`、`P1_lcd_power_en`、`P1_o_lcd_rstn` 四个 LCD GPIO 的处理方式（是否随之移除）。
3. 确认 `constrain.sdc` 中是否有涉及 ch0 DPHY 的时序约束需要同步清理。

### 4.7 最小验证顺序

1. 只读打开 Efinity GUI → Interface Designer → MIPI DPHY 配置，检查 ch0 和 ch1 的独立性和可移除性。
2. 确认 ch0 移除不影响 `dsi_tx_top_inst1` 对 ch1 的驱动。
3. 如 GUI 支持部分移除：通过 GUI 移除 ch0 → 重新生成 periphery → 清理 top.v ch0 端口 → 重跑 Map 确认 DSI 族 EFX-0256 从 73（70 DSI + 3 LCD）降至 0。
4. 如 GUI 不支持：升级为 TBD → 需 Codex2 裁定方案 B（安全 tie-off）。

---

## 5. axi0 边带（10 个）—— 暂维持 EXPECTED

### 5.1 事实

- `axi_target0 is_axi_enable="true"`。
- axi0 的主要数据通道（ARADDR、AWADDR、WDATA、WSTRB 等）已被 `frame_buffer` → `axi_interconnect` → `axi0_*` 顶层端口链正确驱动。
- 仅 10 个边带信号未驱动：`ARAPCMD`、`ARQOS`、`AWALLSTRB`、`AWAPCMD`、`AWCACHE[3:0]`、`AWCOBUF`、`AWQOS`。

### 5.2 结论

**暂维持 EXPECTED。** 这些是 DDR IP 生成的 AXI 边带信号，当前 `axi_interconnect` 未使用（`top.v` 中 cache/prot/qos 端口标记为 `()` 即未连接）。与 AXI1 不同：axi0 的主通道数据流完整驱动，边带信号在实际 DDR 操作中不影响功能。可在 PNR 后根据实际 placement 结果和 DDR 功能测试决定是否需要 tie-off 或通过 IP 配置关闭。

---

## 6. 跨问题依赖与审查顺序

```
审查顺序（按风险隔离）:

第 1 层：LED（独立 GPIO，不涉及任何 IP/Interface Designer）
  └─ 可立即实施（仅 top.v 两行 assign）
  └─ 独立审查门：Codex2 批准后即可修改并重跑 Map

第 2 层：AXI1 / JTAG / MIPI DSI ch0（涉及 Interface Designer）
  └─ 需要 Efinity GUI 操作
  └─ 三者可在 Interface Designer 同一会话中检查
  └─ 但修改应分别审查（每个族的 peri 变更独立可回退）
  └─ 审查顺序建议：
      a. JTAG（确认 jtag_inst2 是否为残留；影响最小）
      b. DSI ch0（确认可移除性）
      c. AXI1（确认 axi_target1 可关闭；影响最大——698 个端口）
```

**禁止交叉依赖**：LED 修复不得阻塞 AXI/JTAG/MIPI 的 Interface Designer 调查，反之亦然。

---

## 7. 全局约束

| 项目 | 约束 |
|---|---|
| 手改 `mem_test.peri.xml` | **禁止**——所有 periphery 变更必须通过 Interface Designer GUI |
| 手改 `mem_test.xml` | **禁止** |
| 手改 `constrain.sdc` | **禁止**（Interface Designer 重新生成后可同步更新） |
| 手改 IP `settings.json` | **禁止** |
| RTL 修改范围 | 仅限 LED tie-off（`top.v` 两行）；DSI/JTAG/AXI 的顶层端口变更必须与 peri 同步 |
| PNR | **继续 HOLD**，直到本方案经 Codex2 审查批准 |
| Map PASS | **≠ warning 可忽略或 M0 PASS** |

---

## 8. NOT VERIFIED

- Efinity Interface Designer GUI 的实际可操作项（本机未打开 GUI）
- 历史 PNR 日志中 JTAG TDO 的 warning 状态
- `P1_lcd_power_en` 和 `P1_o_lcd_rstn` 在 top.v 中的实际驱动状态（需进一步核查——可能有驱动或同属未驱动）
- ch0 移除后 PNR placement 结果
- axi_target1 关闭后 DDR 功能回归
- 板级闭环

---

## 9. 结论

**PNR HOLD。** 四项接口问题中：

| 族 | 旧结论 | 新结论 | 分辨率 |
|---|---|---|---|
| LED | UNEXPECTED | UNEXPECTED（保持） | 最小 RTL 修复（两行 assign） |
| JTAG | TBD | TBD（保持） | 新发现：jtag_inst2 不在 peri.xml 中 |
| AXI1 | ~~EXPECTED~~ | **TBD / BLOCKING** | `is_axi_enable="true"` → 不能标 EXPECTED |
| DSI ch0 | ~~EXPECTED / DSI 整体未激活~~ | **TBD / 仅 ch0 未驱动** | ch1 被 dsi_tx_top_inst1 正常驱动 |
| axi0 边带 | EXPECTED | EXPECTED（暂维持） | 主通道完整驱动 |

**下一步门禁**：Codex2 审查本方案。批准后：
1. 先执行 LED 修复（独立门禁）。
2. 在 Efinity GUI Interface Designer 中逐项检查 JTAG/DSI/AXI1。
3. 根据 GUI 检查结果编写二期决议记录。
4. AXI1/JTAG/DSI 的修改分别审查，各自进入 PNR 验证。
