# Codex Review Packet

## 任务目标

两路摄像头均已接入并写入 DDR，仅需在 top.v 增加一个 2:1 MUX 使 HDMI 可通过 `i_sw[1]` 在通道 0/1 间切换，同时弃用 DSI TX 链路。不新增任何子模块，不改 framebuffer/debayer/AXI，纯顶层走线调整。

## 当前结论

**现状**：
- 双路 CSI RX 已双例化（`soft_mipi_rx_top_inst` S0, `_inst1` S1），I2C S0/S1 独立，DPHY ck0/ck1 成对
- 双路 framebuffer 已双例化（ch0 ADDR=0, ch1 ADDR=0x0180_0000, FB_NUM=3），通过 `axi_m_*[]` 片选拼到 AXI slave[0]/[1]→`axi0`
- 双路 debayer + white_balance 完整链路 (ch0→wb0, ch1→wb1)
- HDMI `hdmi_top_inst` 目前只接 wb1，48bit→24bit 经 `sel` 串行输出
- 板上 P1 LCD 经 DSI TX 接 wb1；P0 LCD 端口未挂载
- `i_sw[0]`=PLL reset，`i_sw[1]` 空闲可用

**方案（按键切换，DSI 弃用）**：

1. **i_sw[1] 通道选择**：在 `hdmi_tx_slow_clk` 域加 `if(i_sw[1])` 选 wb0 或 wb1 的 vs/hs/de/data → `rgb_*_r` → `hdmi_top_inst`。因为 HDMI 时钟域已经用 `always @(posedge hdmi_tx_slow_clk)` 寄存器了输入，MUX 放同一个 always 块内即可。

2. **pixel_data_en 本地生成**：`pixel_data_en` 当前由 `dsi_tx_top_inst1` 输出（内部为 `vid_rst_n = dly_cnt[26]`，在 Panel 配置完成后释放）。弃用 DSI 后需本地生成：
   ```verilog
   // 替代 DSI 生成的 pixel_data_en
   reg [26:0] vid_dly_cnt;
   always @(posedge i_sysclk_div2 or negedge rst_n) begin
       if (!rst_n)
           vid_dly_cnt <= 'd0;
       else if (!vid_dly_cnt[26])
           vid_dly_cnt <= vid_dly_cnt + 1'b1;
   end
   assign pixel_data_en = vid_dly_cnt[26];
   ```
   这个重用了 `rst_n`（即 `rst_cnt[20]`，已聚合 arst_n 等 PLL lock 条件），延迟 2^26 / ~50MHz ≈ 1.34 秒后释放，足够 DDR/时钟稳定。

3. **DSI TX 全部移除**：
   - 删除 `dsi_tx_top_inst1` 及其 `color_bar_rgb_inst`（该 color bar 仅供给 DSI 测试用）
   - 新增 `assign pixel_data_en = vid_dly_cnt[26]` 替换原本 DSI 生成
   - DSI 输出端口 (P1_lcd_*, P0_lcd_*, `mipi_tx_*` 共 60+ 个端口) → 赋值静态安全值：
     - LCD_POWER = 1'b0（关屏供电）
     - LCD_RST = 1'b0（保持复位）
     - mipi_tx 所有 HS/LP/RST OE=0, OUT=0 — 高阻/关断
   - **DSI IP vendor core** (`ip_vendor/dsi_tx/`) 和 `dsi_tx_top.v` 源码**保留不删**，仅不例化。后续若需要可恢复。

4. **无修改范围**：
   - CSI RX 双路：不动
   - framebuffer ×2：不动
   - debayer ×2 + white_balance ×2：不动
   - AXI interconnect ×2（uw/ur）：不动
   - constrain.sdc：不动（hdmi_tx_slow_clk 约束不变，DSI 相关约束无实际影响毕竟端口 tie-off）
   - mem_test.xml：建议移除 `dsi_tx_top.v` 设计文件条目，但可选先注释保留

## 修改或计划涉及的文件

| 文件 | 改动类型 | 改动量 |
|------|----------|--------|
| `final_project/fpga/rtl/top/top.v` | **唯一修改** | ~80 行（删 DSI + color_bar 例化，加 MUX + pixel_data_en 生成，tie-off 端口） |
| `final_project/fpga/efinity/mem_test.xml` | 可选：注释 dsi_tx_top.v 条目 | 1-2 行 |

## 关键模块与信号链路

```
MIPI CAM0 → soft_mipi_rx_top_inst → rx_out_data/hs/vs/de
                                       ↓
          u_frame_buffer(ch0, ADDR=0) → ch0_hs/vs/de/{g,b}
                                       ↓
          debayer_top → rgb_datax2 → u0_white_balance → wb0_* ──┐
                                                                  │
MIPI CAM1 → soft_mipi_rx_top_inst1→ rx_out_data1/hs1/vs1/de1    │
                                       ↓                          │
          u_frame_buffer1(ch1, ADDR=0x1800000) → ch1_hs/vs/de    │
                                       ↓                          │
          debayer_top1 → rgb1_datax2 → u1_white_balance → wb1_*──┤
                                                                  │
                                                 i_sw[1] ────→  MUX (hdmi_tx_slow_clk域)
                                                                  │
                                                         rgb_*_r → hdmi_top_inst → TMDS → PC
```

**跨时钟域**：wb0/wb1 数据跑在 `i_sysclk_div2`，HDMI 输入跑在 `hdmi_tx_slow_clk`（74.25MHz）。当前代码直接用 `always @(posedge hdmi_tx_slow_clk)` 采样 `wb1_*_out`——这实际上是简单的两级同步（时序靠布局布线保证），MUX 加在同一个时钟域后不改变 CDC 性质。

## 时钟、复位、AXI、framebuffer、双通道影响

- **时钟**：不变。hdmi_tx_slow_clk 和 i_sysclk_div2 都是现有时钟，无新增。
- **复位**：`pixel_data_en` 生成由 `rst_n`（聚合 PLL lock + 计数器）驱动，与之前 DSI 生成时序差异约 1.34s（DSI 原需等 Panel config done + dly_cnt）。1.34s 远大于 DDR 初始化时间（`cfg_count` 数到 0xFF），复位时序安全。
- **AXI**：不变。双路 framebuffer 仍各写各读，S_COUNT=3 未变。
- **framebuffer**：`out_sync`（sw_cnt[25]）仍延迟 framebuffer 读使能，与 DSI 无关，不受影响。
- **双通道对称性**：MUX 加在 HDMI 入口，不破坏 ch0/ch1 链路对称性。debayer 和 white_balance 双路保持完全对称。

## DDR 带宽评估

- 单路 1080p RAW 写入：~1920×1080×60×2B ≈ 249 MB/s（RAW 16bpp）
- 双路写入：~498 MB/s
- 单路 framebuffer 读出（O_VID_WIDTH=16）：~249 MB/s
- 双路读出（两路 debayer 都在跑）：~498 MB/s
- **合计**：~1 GB/s
- axi0 (512-bit @ ~300MHz DDR) 理论带宽 ~19.2 GB/s
- 即使 AXI 效率仅 50%，也有 ~9.6 GB/s，带宽**充裕**。不需降采样。

**优化建议（非当前修改范围）**：若未来带宽紧张，可栅控非当前 HDMI 选中路的 framebuffer 读出路径（`frame_en` 置低），每路省 ~249 MB/s。

## 已运行验证
- **命令**：无（纯方案阶段，未修改任何文件）
- **结果**：N/A
- **关键 warning**：N/A

## 机械臂 / 外设状态
- myCobot 是否涉及：否
- 本轮仅涉及 FPGA 视频链路和 HDMI 输出

## 未验证项和风险假设

1. **i_sw[1] 极性**：假设高电平=ch1、低电平=ch0（或反正），后续可配。拨码开关一般为上拉=高=1。
2. **两路摄像头 I2C 地址相同**：SC431HAI2 默认 I2C 地址固定，S0/S1 总线独立所以不会冲突。已验证 top.v 中 S0/S1 的 I2C 是独立 IO。
3. **DSI TX IP 保留但不例化**：Efinity 综合时若 `dsi_tx_top.v` 仍在 `mem_test.xml` 但顶层未例化，应被优化掉为 unused。若报 warning 可后续从 XML 删除条目。
4. **hdmi_tx_slow_clk 采样 wb 信号**：当前只有简单单寄存器采样（`always @(posedge hdmi_tx_slow_clk)`），无 FIFO/async FIFO。i_sysclk_div2 (~50MHz) 和 hdmi_tx_slow_clk (74.25MHz) 的频率差异小，像素时序有 vs/hs/de 作为帧对齐锚点，meta-stability 风险低但存在。如果需要严格 CDC，需异步 FIFO——但这超出了本次"按键切换"的小改动范围，且当前单路模式也依赖同一 CDC 机制，已在实测中工作。

## 希望 Codex 判断的问题

1. **pixel_data_en 本地生成方案**：用 `rst_n` + 27-bit 计数器替代 DSI 的 `vid_rst_n`。延时 2^26 周期 ≈ 1.34s，是否足够让 DDR 和整个视频链路稳定？还是应该更保守（更大的计数器位宽）？

2. **DSI 端口 tie-off 策略**：60+ 个 DSI TX 端口在顶层 tie-off 为安全静态值，是否有遗漏风险（如某个端口必须配置特定电平才能避免板上短路或漏电）？

3. **AXI S_COUNT=3 但只用 2 路**：当前 axi_interconnect S_COUNT=3 但只有 slave[0](ch0) 和 slave[1](ch1) 连接，slave[2] 未连接。这是否已经过综合验证？是否会导致未连接的 axi_m_* 信号被优化或产生 warning？

4. **两路 framebuffer 均带 `out_sync` 延迟**：`out_sync` 经同一 `sw_cnt[25]` 产生（约 0.67 秒），两个 framebuffer 的读启动完全同步。这是否会在 DDR 控制器产生瞬时 burst 争抢？是否需要 stagger 两路的 `out_sync`（比如 ch1 延迟 1 帧再启动）？
