# M0 EFX-0256 接口归属静态审计

> 日期：2026-07-15
> 来源 Agent：Claude（Fable 5，按 Codex2 指令执行静态清点；2026-07-15 修正版）
> 工程：`competition_project_single_camera/mem_test.xml`
> 范围：仅对 Map 综合阶段 785 个 EFX-0256 WARNING 做顶层端口族分类和静态归属判断。不修改 RTL、SDC、XML、IP 或 CPU 代码。不运行 PNR/STA/CDC/bitstream。
> 结论：**PNR HOLD，直到接口归属经 Codex2 审查。本修正版替换 2026-07-15 初版。**

---

## 修正记录（2026-07-15，Codex2 指令）

| 修正项 | 初版 | 修正版 | 依据 |
|---|---|---|---|
| AXI1 698 个 | EXPECTED | **TBD / BLOCKING** | `mem_test.peri.xml:455`：`axi_target1 is_axi_enable="true"`——活跃 DDR 目标，不能标 EXPECTED |
| MIPI DSI 70 个 | "整个 DSI 未激活" | **仅 ch0 未驱动** | `top.v:1392-1448`：`dsi_tx_top_inst1` 实际驱动 ch1（`ck1/dp10-dp13`）；ch0（`ck0/dp00-dp03`）孤立的未驱动通道 |
| JTAG | 未区分 peri 声明状态 | **jtag_inst2 不在 peri.xml 中** | `mem_test.peri.xml` 全文搜索无 `jtag_inst2`；仅 `jtag_inst1`（JTAG_USER1）有声明 |
| "无 DDR 主通道" | 声称 785 中无 DDR 主通道问题 | **删除此表述** | AXI1 698 个均属于 DDR IP 的 `axi_target1` |
| "224 warning" | 出现该旧数字 | **改为 223 个带 code 的 VERI/VDB WARNING** | 修正后的正确统计 |

---

## 1. 数据来源

- 日志文件：`outflow/mem_test.warn.log`（整文件 1177 行，1173 非空行；本次运行段 1043 行，1042 非空行）
- EFX-0256 格式：`[EFX-0256 WARNING] The primary output port '<name>' wire '<name>' is not driven.`
- 全部 785 个事件均为顶层 primary output port 未被任何逻辑驱动。
- 另有 223 个带 code 的 VERI/VDB WARNING（15 类别），与 785 个 EFX-0256 是独立的两类 warning。

---

## 2. 端口族分类汇总

| 族 | 数量 | 占比 | 本次结论 |
|---|---|---|---|
| axi1（AXI1 主机接口，全部信号） | 698 | 88.9% | **TBD / BLOCKING** |
| MIPI DSI ch0（mipi_tx ck0/dp00–dp03） | 70 | 8.9% | **TBD / 仅 ch0 未驱动** |
| axi0（AXI0 控制面边带信号） | 10 | 1.3% | EXPECTED（暂维持） |
| LCD/外设控制 | 3 | 0.4% | TBD（与 DSI ch0 关联） |
| JTAG（jtag_inst1/2 TDO） | 2 | 0.3% | **TBD** |
| LED | 2 | 0.3% | **UNEXPECTED** |
| **合计** | **785** | 100% | |

---

## 3. 逐族明细

### 3.1 axi1 — 698 个（TBD / BLOCKING）

**覆盖信号：**

| 信号组 | 位宽 | 数量 |
|---|---|---|
| ARADDR | [32:0] | 33 |
| AWADDR | [32:0] | 33 |
| WDATA | [511:0] | 512 |
| WSTRB | [63:0] | 64 |
| ARID | [5:0] | 6 |
| AWID | [5:0] | 6 |
| ARLEN | [7:0] | 8 |
| AWLEN | [7:0] | 8 |
| ARSIZE | [2:0] | 3 |
| AWSIZE | [2:0] | 3 |
| ARBURST | [1:0] | 2 |
| AWBURST | [1:0] | 2 |
| AWCACHE | [3:0] | 4 |
| ARVALID | — | 1 |
| AWVALID | — | 1 |
| WVALID | — | 1 |
| WLAST | — | 1 |
| BREADY | — | 1 |
| RREADY | — | 1 |
| ARLOCK | — | 1 |
| AWLOCK | — | 1 |
| ARAPCMD | — | 1 |
| AWAPCMD | — | 1 |
| ARQOS | — | 1 |
| AWQOS | — | 1 |
| AWALLSTRB | — | 1 |
| AWCOBUF | — | 1 |

**示例端口：** `axi1_AWADDR[31]`、`axi1_WDATA[0]`、`axi1_AWVALID`、`axi1_BREADY`

**当前所有权：** 顶层端口声明存在，但在当前视频-only 工程中，axi1 主机接口整体未被任何内部逻辑驱动（无 AXI master IP 接入本接口）。

**periphery/约束依据：**

`mem_test.peri.xml:455`：
```xml
<efxpt:axi_target1 is_axi_width_256="false" is_axi_enable="true">
```

axi1 的全套 AXI 信号均属于 DDR IP（`ddr_inst1`）的 `axi_target1`，该 target **已使能**（`is_axi_enable="true"`）。这不是闲置预留端口——DDR IP 期望有 AXI master 驱动该目标。

**当前结论：TBD / BLOCKING。** 旧版审计将此项标为 `EXPECTED`——该结论**错误**。`is_axi_enable="true"` 意味着 698 个 axi1 信号是 DDR 控制器的活跃 AXI 目标端口。不驱动这些信号在 PNR 阶段极可能导致 placement 失败或工具内部断言（参考 `final_project` 的 2288/1776 IO outpad 失败模式）。

**修复方向（本次不实施）**：
- **首选**：通过 Efinity Interface Designer 将 `axi_target1` 的 `is_axi_enable` 改为 `false`，从根源消除 698 个端口。
- **备选**：引入真实 AXI master IP（不在 M0 范围内）。
- **禁止**：手工 tie-off 698 位信号（不可维护，AXI 协议要求 valid/ready 握手，硬 tie-off 导致协议死锁）。
- **禁止**：手改 `mem_test.peri.xml`（必须通过 Interface Designer GUI）。

详见：`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md` §3。

---

### 3.2 MIPI DSI ch0 — 70 个（TBD / 仅 ch0 未驱动）

**覆盖信号（5 lanes × 14 signals/lane）：**

| Lane | 信号 |
|---|---|
| ck0 | HS_OE, HS_OUT[7:0], LP_N_OE, LP_N_OUT, LP_P_OE, LP_P_OUT, RST |
| dp00 | HS_OE, HS_OUT[7:0], LP_N_OE, LP_N_OUT, LP_P_OE, LP_P_OUT, RST |
| dp01 | HS_OE, HS_OUT[7:0], LP_N_OE, LP_N_OUT, LP_P_OE, LP_P_OUT, RST |
| dp02 | HS_OE, HS_OUT[7:0], LP_N_OE, LP_N_OUT, LP_P_OE, LP_P_OUT, RST |
| dp03 | HS_OE, HS_OUT[7:0], LP_N_OE, LP_N_OUT, LP_P_OE, LP_P_OUT, RST |

**示例端口：** `mipi_tx_ck0_HS_OE`、`mipi_tx_dp00_HS_OUT[7]`、`mipi_tx_dp03_RST`

**当前所有权：**

核查结论：`top.v` 同时声明 ch0（`mipi_tx_ck0/dp00-dp03`，lines 297-331）和 ch1（`mipi_tx_ck1/dp10-dp13`，lines 332-366）两套 MIPI DSI TX 输出端口。

`dsi_tx_top_inst1`（`top.v:1392`）**实际驱动 ch1**——其全部 MIPI 端口连接指向 `mipi_tx_ck1_*` 和 `mipi_tx_dp10_*` 到 `mipi_tx_dp13_*`，像素数据来自 ch0 摄像头经 white_balance 后进入 DSI TX 通路。

ch0（`mipi_tx_ck0_*`、`mipi_tx_dp00_*` 到 `mipi_tx_dp03_*`）在 `top.v` 中**没有任何模块驱动**。

**periphery/约束依据：** `mem_test.peri.xml` 同时声明了 ch0（`GPIOT_PN_23/27/26/25/24`）和 ch1（`GPIOT_PN_28/...`）两套 MIPI DPHY TX lane。

**当前结论：TBD / 仅 ch0 未驱动。** 旧版审计描述"整个 DSI 未激活"——**该结论错误**。ch1 通过 `dsi_tx_top_inst1` 有完整的数据路径驱动，ch0 才是完全孤立的未驱动通道。70 个 EFX-0256 全部属于 ch0。M1 如需裁剪 DSI，目标应为 ch0 和关联 LCD GPIO，而非 ch1。

**修复方向（本次不实施）**：
- **首选**：通过 Interface Designer 移除 ch0 的 5 个 DPHY lane，并同步清理 `top.v` 端口。
- **备选**：在 `top.v` 为 ch0 端口赋安全复位值（HS/LP 输出和 OE 信号清零）。

详见：`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md` §4。

---

### 3.3 axi0 — 10 个（EXPECTED，暂维持）

**覆盖信号：**

| 信号 | 数量 |
|---|---|
| ARAPCMD | 1 |
| ARQOS | 1 |
| AWALLSTRB | 1 |
| AWAPCMD | 1 |
| AWCACHE[3:0] | 4 |
| AWCOBUF | 1 |
| AWQOS | 1 |

**示例端口：** `axi0_AWCACHE[0]`、`axi0_ARQOS`、`axi0_AWAPCMD`

**当前所有权：** axi0 的主要数据通道（ARADDR、AWADDR、WDATA 等）已被 `frame_buffer` → `axi_interconnect` → `axi0_*` 顶层端口链正确驱动。这 10 个信号是 AXI 协议的辅助/边带信号，当前 `axi_interconnect` 未使用（`top.v` 中 cache/prot/qos 端口标记为未连接 `()`）。

**periphery/约束依据：** `axi_target0 is_axi_enable="true"`，但主通道完整驱动。

**当前结论：EXPECTED（暂维持）。** 与 axi1 不同：axi0 的主通道数据流完整驱动，边带信号在实际 DDR 操作中不影响功能。可在 PNR 后根据实际 placement 结果和 DDR 功能测试决定是否需要通过 IP 配置关闭这些边带。

---

### 3.4 LCD/外设控制 — 3 个（TBD，与 DSI ch0 关联）

| 端口 | 说明 |
|---|---|
| `P0_lcd_power_en` | LCD 面板电源使能（ch0 配套） |
| `P0_lcd_rstp` | LCD 面板复位，正极性（ch0 配套） |
| `P1_o_lcd_rstn` | LCD 面板复位，负极性（ch1 配套） |

**示例端口：** `P0_lcd_power_en`、`P0_lcd_rstp`

**当前所有权：** `dsi_tx_top_inst1` 驱动 `P1_lcd_power_en`（`LCD_POWER` 端口）和 `P1_lcd_rstp`（`LCD_RST_P` 端口），但 `P0_lcd_power_en` 和 `P0_lcd_rstp`（ch0 配套）未被驱动。`P1_o_lcd_rstn` 在 `top.v:194` 声明，也需进一步确认驱动状态。

**periphery/约束依据：** `mem_test.peri.xml` 中 `P0_lcd_power_en`（`GPIOT_N_22`）、`P0_lcd_rstp`（`GPIOT_N_21`）、`P1_lcd_power_en`（`GPIOT_N_20`）、`P1_o_lcd_rstn`（`GPIOT_P_20`）均已声明为 output。

**当前结论：TBD。** 这些信号与 DSI ch0/ch1 配套。若 ch0 通过 Interface Designer 移除，`P0_*` 信号应随之移除。`P1_*` 信号的驱动状态须在移除 ch0 后独立核查。

---

### 3.5 JTAG — 2 个（TBD）

| 端口 | 说明 |
|---|---|
| `jtag_inst1_TDO` | JTAG USER1 实例的 TDO 输出 |
| `jtag_inst2_TDO` | JTAG USER2 实例的 TDO 输出 |

**示例端口：** `jtag_inst1_TDO`、`jtag_inst2_TDO`

**当前所有权：**

核查结论：
- `mem_test.peri.xml` **仅有 `jtag_inst1`**（`JTAG_USER1`，line 393），包含 TDO 在内的完整 gen_pin 声明。
- `mem_test.peri.xml` **无 `jtag_inst2`** 的任何声明（全文搜索 `jtag_inst2` 无匹配）。
- `top.v` 同时声明 `jtag_inst1_TDO`（line 118）和 `jtag_inst2_TDO`（line 119）作为输出端口，以及 `jtag_inst2_*` 的 JTAG 输入信号（lines 75-84）。

**periphery/约束依据：** `jtag_inst1` 有完整 peri 声明（JTAG_USER1），Debugger 配置存在（`*.dbg.vdb`、`debugger_templates/`）。`jtag_inst2` 在 peri 中无声明，但其顶层端口和输入信号存在于 RTL 中——可能是 Interface Designer 历史操作的残留端口。

**当前结论：TBD。两者均保持 TBD。** 关键发现：`jtag_inst2` 不在 `mem_test.peri.xml` 中，不属于"两个 JTAG 实例均已声明"的情况。`jtag_inst1_TDO` 可能由 JTAG 硬核在 PNR 阶段自动连接，但需要同版本工具的历史 PNR 证据确认。**这两个不是"axi1 高位"**——它们是独立的 JTAG 端口族。

**修复方向（本次不实施）**：
- `jtag_inst1_TDO`：需要 Interface Designer 或同版本历史 PNR 证据确认工具自动连接机制。
- `jtag_inst2`：裁定是否为过期顶层端口。如确认是残留，通过 Interface Designer 清理。

详见：`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md` §2。

---

### 3.6 LED — 2 个（UNEXPECTED）

| 端口 | 说明 |
|---|---|
| `led[3]` | LED3 |
| `led[2]` | LED2 |

**示例端口：** `led[3]`、`led[2]`

**当前所有权：**

核查结论：
- `top.v:607`：`assign led[0] = ~ddr_cfg_ok`（D14，DDR 配置完成指示）。
- `top.v:608`：`assign led[1] = vs_cnt[5]`（D15，帧计数闪烁）。
- `led[3:2]` 在 RTL 中**没有任何逻辑驱动**。
- `mem_test.peri.xml:169-173`：`led[3:0]` 四位置全部声明为实体 GPIO 输出（`mode="output"`，`io_standard="1.8 V LVCMOS"`）。

**periphery/约束依据：** 四位 LED 全部有物理引脚绑定（`GPIOB_N_30`、`GPIOB_P_24`、`GPIOB_N_24`、`GPIOB_P_17`）。

**当前结论：UNEXPECTED。** 不同于 axi1/DSI 族的复杂 IP 端口，LED 是简单 GPIO 输出，不应在综合后仍悬浮。`top.v` 驱动了 `led[1:0]`，但 `led[3:2]` 位被 periphery 绑定为实体引脚但无 RTL 驱动。

**修复方向（本次不实施）**：
- **首选**：在 `top.v` 追加 `assign led[3] = 1'b0;` 和 `assign led[2] = 1'b0;`——最小安全 tie-off。
- 详见：`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md` §1。

---

## 4. 汇总判断

| 族 | 数量 | 旧结论（初版） | 修正后结论 | 关键依据 |
|---|---|---|---|---|
| axi1（全部 AXI1 信号） | 698 | ~~EXPECTED~~ | **TBD / BLOCKING** | `is_axi_enable="true"` |
| MIPI DSI ch0 | 70 | ~~EXPECTED / DSI 整体未激活~~ | **TBD / 仅 ch0 未驱动** | ch1 被 `dsi_tx_top_inst1` 驱动 |
| axi0（边带信号） | 10 | EXPECTED | EXPECTED（暂维持） | 主通道完整驱动 |
| LCD/外设控制 | 3 | ~~EXPECTED~~ | TBD | 与 DSI ch0 关联 |
| JTAG（jtag_inst1/2_TDO） | 2 | TBD | **TBD** | jtag_inst2 不在 peri.xml 中 |
| LED（led[3:2]） | 2 | UNEXPECTED | **UNEXPECTED** | top.v 不驱动 led[3:2]；periphery 声明所有 4 位 |

---

## 5. 结论

**PNR HOLD，直到接口归属经 Codex2 审查。**

重点待审查项：
1. **axi1 698 个**：`is_axi_enable="true"` 的活跃 DDR 目标。不能标 EXPECTED。必须通过 Interface Designer 处理（关闭 target1 或接入 master）。禁止手工 tie-off。
2. **MIPI DSI ch0 70 个**：ch1 由 `dsi_tx_top_inst1` 正常驱动，ch0 是孤立的未驱动通道。不是"整个 DSI 未激活"。需通过 Interface Designer 移除 ch0 DPHY。
3. **jtag_inst1_TDO / jtag_inst2_TDO**：jtag_inst2 不在 `mem_test.peri.xml` 中——这是新发现。两者都需要 Interface Designer 审查或历史 PNR 证据。
4. **led[3:2]**：简单 GPIO 悬浮不可接受。最小修复：在 `top.v` 赋安全常量。

修复方案详见：`docs/review_packets/m0_pnr_interface_resolution_plan_20260715.md`。

---

## NOT VERIFIED

- PNR/STA/CDC/bitstream — 均未运行
- Interface Designer GUI 的实际可操作项
- 历史 PNR 中 jtag_inst1/2_TDO 的 warning 状态
- `axi_target1` 是否可在 GUI 中关闭
- DSI ch0 DPHY 是否可在 GUI 中独立移除
- 板级闭环
