# final_project Current Code Architecture

> 历史快照（2026-07-07）。本文中的“当前”仅表示当日工作树；2026-07-18 团队已取消双摄方案并固定 `competition_project_single_camera/` 单摄 J48/ch0 为唯一正式视频/识别路线。请优先阅读根目录 `CURRENT_STATE.md`。

Date: 2026-07-07

Scope: this document describes the current code under `CICC/final_project` only. It is based on a read of the present working tree, mainly:

- `fpga/efinity/mem_test.xml`
- `fpga/efinity/constrain.sdc`
- `fpga/rtl/top/top.v`
- `fpga/rtl/mipi_csi/soft_mipi_rx_top.v`
- `fpga/rtl/framebuffer/frame_buffer.v`
- `fpga/rtl/debayer/debayer_top_2to1.v`
- `fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`
- `fpga/rtl/dvi_tx/hdmi_top.v`
- `cpu/app/src/*.c`
- `cpu/app/include/bsp.h`
- `integration/*.md`

The current implemented system is primarily an FPGA video bring-up path:

```text
dual MIPI CSI cameras
  -> RAW capture
  -> DDR framebuffer
  -> debayer
  -> HDMI channel select
  -> DVI/HDMI TMDS output
```

CPU-side classification, task matching, OSD, feature extraction, and myCobot closed-loop control are still mostly skeletons or interface drafts.

## 1. Project Entry and Build Shape

The Efinity project entry is `fpga/efinity/mem_test.xml`.

Project facts:

| Item | Current value |
|---|---|
| Efinity project | `mem_test` |
| Top module | `top` |
| Device family | `Titanium` |
| Device | `TJ375N529` |
| Timing model | `I3` |
| Efinity version in XML | `2025.2.288.4.15` |
| Main constraint file | `fpga/efinity/constrain.sdc` |

The active compile-time switches in `fpga/rtl/top/top.v` are:

```verilog
`define FRAME_BUFFER
`define HDMI_OUT_EN
```

Inactive compile-time switches:

```verilog
// `define CONTRAST_BRIGHT_EN
// `define UVC_EN
```

Important implication:

- The framebuffer path and HDMI output path are active.
- Contrast/brightness adjustment is not active.
- UVC/YUV path is not active.
- MIPI DSI TX files are still present in the Efinity project, but the active top-level design ties DSI/LCD/MIPI-TX outputs off.

## 2. Top-Level Parameters

`top.v` defines the key video and bus parameters:

| Parameter | Value | Meaning |
|---|---:|---|
| `AXI_DATA_WIDTH` | `512` | External DDR/AXI data width |
| `AXI_ADDR_WIDTH` | `33` | External DDR/AXI address width |
| `AXI_ID_WIDTH` | `6` | AXI ID width |
| `S_COUNT` | `2` | Two framebuffer channels feed the AXI interconnect |
| `M_COUNT` | `1` | One external AXI memory target |
| `I_VID_WIDTH` | `32` | Framebuffer input width, four RAW8 samples per beat |
| `O_VID_WIDTH` | `16` | Framebuffer output width, two RAW8 samples per beat |
| `PACK_BIT` | `40` | CSI packed RAW data width from top-level wrapper |
| `WR_FIFO_DEPTH` | `256` | Framebuffer write FIFO depth at top parameter level |
| `RD_FIFO_DEPTH` | `256` | Framebuffer read FIFO depth at top parameter level |
| `FB_NUM` | `3` | Triple-buffer addressing |
| `BURST_LEN` | `63` | AXI burst length parameter used by framebuffer |
| `MAX_VID_WIDTH` | `1920` | Full horizontal active video |
| `MAX_VID_HIGHT` | `1080` | Full vertical active video |
| `HACT` | `1920` | HDMI target active width |
| `VACT` | `1080` | HDMI target active height |
| `HSP` | `4` | Full-rate horizontal sync |
| `HBP` | `88` | Full-rate horizontal back porch |
| `HFP` | `120` | Full-rate horizontal front porch |
| `VSP` | `2` | Vertical sync |
| `VBP` | `20` | Vertical back porch |
| `VFP` | `20` | Vertical front porch |

The framebuffer output timing is half-width horizontally because it is a two-pixel-per-clock stream:

| Timing passed to framebuffer output | Value |
|---|---:|
| `HDMI_H_FRONT_PORCH = HFP >> 1` | `60` |
| `HDMI_H_SYNC = HSP >> 1` | `2` |
| `HDMI_H_VALID = HACT >> 1` | `960` |
| `HDMI_H_BACK_PORCH = HBP >> 1` | `44` |
| `HDMI_V_FRONT_PORCH = VFP` | `20` |
| `HDMI_V_SYNC = VSP` | `2` |
| `HDMI_V_VALID = VACT` | `1080` |
| `HDMI_V_BACK_PORCH = VBP` | `20` |

So the design regenerates `960 x 1080` timing in the two-pixel domain, then expands it to `1920 x 1080` in the HDMI one-pixel domain.

## 3. Clock Domains

The important clock constraints in `constrain.sdc` are:

| Clock signal | Period | Approx. freq | Main role |
|---|---:|---:|---|
| `pll_inst1_CLKOUT0` | `13.468 ns` | `74.25 MHz` | Legacy/video-related clock input |
| `CLK_5M` | `200.337 ns` | `5 MHz` | Low-speed peripheral clock |
| `mipi0_ref_clk` | `37.037 ns` | `27 MHz` | MIPI reference clock constraint |
| `pll_inst2_CLKOUT0` | `5.000 ns` | `200 MHz` | DDR/AXI-related generated clock |
| `axi0_ACLK` | `5.000 ns` | `200 MHz` | External DDR AXI port 0 |
| `i_fb_clk` | `40.000 ns` | `25 MHz` | Reset delay and DDR config FSM |
| `mipi_clk` | `10.000 ns` | `100 MHz` | CSI controller config/I2C clock |
| `i_sysclk_div2` | `14.286 ns` | `70 MHz` | CSI pixel, framebuffer video, debayer, WB domain |
| `hdmi_tx_slow_clk` | `7.143 ns` | `140 MHz` | HDMI pixel and TMDS encoder slow clock |
| `hdmi_tx_fast_clk` | `1.429 ns` | `700 MHz` | HDMI serializer/fast clock |
| `mipi_rx_ck0_CLKOUT` | `10.000 ns` | `100 MHz` | CSI receiver byte clock ch0 |
| `mipi_rx_ck1_CLKOUT` | `10.000 ns` | `100 MHz` | CSI receiver byte clock ch1 |
| `mipi_dphy_tx_FASTCLK_C/D` | `2.000 ns` | `500 MHz` | DSI TX related, currently tied off at top |
| `mipi_dphy_tx_SLOWCLK` | `8.000 ns` | `125 MHz` | DSI TX related, currently tied off at top |

Main CDC boundaries:

| Boundary | Mechanism |
|---|---|
| CSI D-PHY byte clock to CSI pixel clock | handled inside vendor `csi_rx_controller` |
| `i_sysclk_div2` video write domain to `axi0_ACLK` | write-side FIFO inside framebuffer/DDR path |
| `axi0_ACLK` to `i_sysclk_div2` video read domain | `fifo_d512t128` and output-side buffering |
| `i_sysclk_div2` dual-pixel video to `hdmi_tx_slow_clk` one-pixel video | `video_2pix_to_1pix_cdc` dual-clock FIFO |

## 4. Reset and Bring-Up Chain

Top-level reset flow:

```text
i_sw[0]
  -> sys_pll_rstn / ddr_pll_rstn / MIPI_TX_PLL_RSTN / pll_byteclk_rstn
  -> sys_pll_lock & ddr_pll_lock & pll_byteclk_locked
  -> arst_n
  -> 21-bit delay in i_fb_clk
  -> rst_n
  -> DDR config FSM
  -> ddr_cfg_ok / sys_rst_n
  -> 27-bit delay in i_sysclk_div2
  -> pixel_data_en
```

Important details:

- `arst_n = sys_pll_lock & ddr_pll_lock & pll_byteclk_locked`.
- `MIPI_TX_PLL_LOCKED` is not part of `arst_n`, because DSI TX has been decommissioned.
- `ddr_cfg_ok` becomes true only after `ddr_inst_CFG_DONE`.
- `pixel_data_en = vid_dly_cnt[26]` is the main release signal for the active video chain.

`pixel_data_en` releases/resets:

- both framebuffer instances,
- both AXI interconnect instances,
- debayer,
- white balance instances,
- HDMI CDC bridges.

LED meanings:

| LED | Meaning |
|---|---|
| `led[0]` | `~ddr_cfg_ok`; on means DDR config is not done |
| `led[1]` | `vs_cnt[5]`; divided S0 CSI VS activity |
| `led[2]` | `channel_sel`; selected HDMI camera channel |
| `led[3]` | `hdmi_input_stable & selected_csi_format_ok` |

## 5. Active FPGA Video Pipeline

### 5.1 Top-Level Dataflow

Current visible chain:

```text
S0 MIPI CSI lanes
  -> soft_mipi_rx_top
  -> rx_out_vs/hs/de/data
  -> RAW10 to RAW8 truncation
  -> frame_buffer ch0
  -> debayer ch0
  -> white_balance ch0, instantiated but bypassed
  -> video_2pix_to_1pix_cdc ch0
  \
   -> SW4 selected HDMI stream
  /
S1 MIPI CSI lanes
  -> soft_mipi_rx_top
  -> rx_out_vs1/hs1/de1/data1
  -> RAW10 to RAW8 truncation
  -> frame_buffer ch1
  -> debayer ch1
  -> white_balance ch1, instantiated but bypassed
  -> video_2pix_to_1pix_cdc ch1

selected stream
  -> hdmi_top
  -> dvi_encoder
  -> TMDS outputs
```

`i_sw[1]` is used as a channel toggle button:

- `channel_sel = 0`: select S0/ch0.
- `channel_sel = 1`: select S1/ch1.
- `led[2] = channel_sel`.

Potential issue: comments say the SW4 debounce is about 7.5 ms at 140 MHz, but the implementation updates `sw4_stable` as soon as a different sampled value is seen, while `sw4_cnt` is not used to delay acceptance of the new stable value. Treat this as a possible switch-bounce bug if channel selection toggles unexpectedly.

### 5.2 CSI Receive Stage

Module: `fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

Each camera instantiates one `soft_mipi_rx_top`:

| Channel | Top instance | Camera pins | MIPI pins |
|---|---|---|---|
| S0/ch0 | `soft_mipi_rx_top_inst` | `S0_io_cam_*` | `mipi_rx_ck0_*`, `mipi_rx_dp00..03_*` |
| S1/ch1 | `soft_mipi_rx_top_inst1` | `S1_io_cam_*` | `mipi_rx_ck1_*`, `mipi_rx_dp10..13_*` |

Inside each wrapper:

- `csi_rx_controller` vendor IP receives the 4 MIPI data lanes.
- `i2c_master_ctrl_top` performs camera I2C register initialization.
- `vid_info_det` exists as a monitor, but its outputs are not used by top-level control.

CSI wrapper outputs:

| Signal | Width | Domain | Meaning |
|---|---:|---|---|
| `rx_out_vs` / `rx_out_vs1` | `1` | `i_sysclk_div2` | CSI VC0 frame sync |
| `rx_out_hs` / `rx_out_hs1` | `1` | `i_sysclk_div2` | CSI VC0 line sync |
| `rx_out_de` / `rx_out_de1` | `1` | `i_sysclk_div2` | CSI pixel data valid |
| `rx_out_data` / `rx_out_data1` | `40` | `i_sysclk_div2` | low 40 bits of CSI pixel data |
| `rx_out_datatype` / `rx_out_datatype1` | `6` | `i_sysclk_div2` | CSI datatype |
| `rx_out_pixel_per_clk` / `rx_out_pixel_per_clk1` | `4` | `i_sysclk_div2` | pixels per CSI output clock |

Top checks format with:

```verilog
CSI_RAW10_DATATYPE = 6'h2B
ch*_csi_format_ok = (rx_out_datatype == 6'h2B) &
                    (rx_out_pixel_per_clk == 4'd4)
```

### 5.3 RAW10 to RAW8 Truncation

`top.v` converts each 40-bit RAW10 group to 32-bit RAW8 group:

```verilog
raw10_4pix_to_raw8_4pix = {
  raw10_4pix[39:32],
  raw10_4pix[29:22],
  raw10_4pix[19:12],
  raw10_4pix[9:2]
};
```

Meaning:

- CSI outputs 4 RAW10 pixels per cycle.
- The design drops the two LSBs of each RAW10 sample.
- Framebuffer input becomes `32b = 4 x RAW8`.

This is intentional in current wiring, but it reduces sensor precision from 10-bit to 8-bit before DDR/debayer.

### 5.4 Framebuffer Stage

Module: `fpga/rtl/framebuffer/frame_buffer.v`

There are two framebuffer instances:

| Channel | Instance | DDR start address |
|---|---|---:|
| ch0/S0 | `u_frame_buffer` | `33'h0000_0000` |
| ch1/S1 | `u_frame_buffer1` | `33'h0_0180_0000` |

Common configuration:

| Setting | Value |
|---|---:|
| `AXI_DATA_WIDTH` | `512` |
| `AXI_ADDR_WIDTH` | `33` |
| `I_VID_WIDTH` | `32` |
| `O_VID_WIDTH` | `16` |
| `FB_NUM` | `3` |
| `BURST_LEN` | `63` |
| input clock | `i_sysclk_div2` |
| output clock | `i_sysclk_div2` |
| AXI clock | `axi0_ACLK` |

Frame write path:

```text
CSI vs/hs/de + 32b RAW8 group
  -> vid_rx_align_v1
  -> 512b aligned write FIFO payload
  -> ddr_buffer
  -> ddr_wr_buffer
  -> AXI write channel
```

Frame read path:

```text
AXI read 512b
  -> ddr_rd_buffer
  -> fifo_d512t128
  -> 128b chunks
  -> par2ser_parse / vid_par2ser
  -> 16b RAW pair
  -> data_tx regenerated timing
```

Framebuffer outputs:

| Signal | Width | Domain | Meaning |
|---|---:|---|---|
| `ch*_vs` | `1` | `i_sysclk_div2` | regenerated frame sync |
| `ch*_hs` | `1` | `i_sysclk_div2` | regenerated line sync |
| `ch*_de` | `1` | `i_sysclk_div2` | regenerated active video |
| `{ch*_g,ch*_b}` | `16` | `i_sysclk_div2` | two RAW8 Bayer samples |
| `ch*_frame_ready` | `1` | `i_sysclk_div2` | frame is available and output is enabled |
| `ch*_fifo_underflow` | `1` | `i_sysclk_div2` | output FIFO underflow latched for current frame |

Naming warning: `ch*_g` and `ch*_b` are not green/blue color channels here. They are raw byte containers before debayer.

### 5.5 AXI/DDR Sharing

Both framebuffers share external AXI port 0.

The top-level design instantiates two `axi_interconnect` blocks:

| Instance | Purpose | External port |
|---|---|---|
| `uw_axi_interconnect` | combines write channels from both framebuffers | `axi0_AW*`, `axi0_W*`, `axi0_B*` |
| `ur_axi_interconnect` | combines read channels from both framebuffers | `axi0_AR*`, `axi0_R*` |

`axi1` is tied off:

- `axi1_ARVALID = 0`
- `axi1_AWVALID = 0`
- `axi1_WVALID = 0`
- `axi1_RREADY = 0`
- `axi1_BREADY = 0`
- address/data/strobe fields are zeroed.

So current active DDR traffic is only on AXI port 0.

### 5.6 Debayer Stage

Module: `fpga/rtl/debayer/debayer_top_2to1.v`

Each channel has one debayer:

| Channel | Instance | Input |
|---|---|---|
| ch0 | `debayer_top` | `ch0_vs/hs/de`, `ch0_bayer_2pix` |
| ch1 | `debayer_top1` | `ch1_vs/hs/de`, `ch1_bayer_2pix` |

Top-level Bayer pair construction:

```verilog
CH0_BAYER_SWAP_PIXELS = 1'b1
CH1_BAYER_SWAP_PIXELS = 1'b1

ch*_bayer_2pix = {ch*_b, ch*_g}
```

Debayer input/output:

| Signal | Width | Meaning |
|---|---:|---|
| `raw_datax4_i` | `16` | two RAW8 samples per clock, despite old name |
| `rgb_datax2_o` | `48` | two RGB888 pixels per clock |
| `rgb_bout/gout/rout` | `16` each | two 8-bit samples for each color |

Internal stages:

```text
rgb_gain instance exists
  -> line_buffer
  -> raw_to_rgb
  -> packed 48b BGR two-pixel output
```

Important implementation detail:

- `rgb_gain` is instantiated and receives the raw stream.
- But `line_buffer` uses `raw_datax4_i` directly, not `w_gain_data`.
- Therefore current HDMI output is not affected by `r_gain/g_gain/b_gain`.

Output packing in `debayer_top_2to1` is BGR:

```verilog
rgb_datax2_o = {
  b_odd, g_odd, r_odd,
  b_even, g_even, r_even
};
```

Top-level repacks it to RGB:

```verilog
rgb*_data_rgb = {
  r_odd, g_odd, b_odd,
  r_even, g_even, b_even
};
```

### 5.7 White Balance Stage

Module: `fpga/rtl/uvc_src/white_balance.v`

Two white balance instances exist:

- `u0_white_balance`
- `u1_white_balance`

However HDMI currently bypasses them:

```verilog
localparam HDMI_BYPASS_WHITE_BALANCE = 1'b1;
```

Active HDMI data is:

```text
debayer RGB -> top-level RGB repack -> hdmi*_data_out
```

White balance is connected but inactive for HDMI muxing. Synthesis may optimize the instances away because the bypass selector is a constant.

### 5.8 DSI/MIPI TX Tie-Off

The old DSI display path is not active.

Top-level tie-off behavior:

- `P0_lcd_power_en = 0`
- `P0_lcd_rstp = 0`
- `P1_lcd_power_en = 0`
- `P1_o_lcd_rstn = 0`
- all `mipi_tx_*_HS_OE = 0`
- all `mipi_tx_*_HS_OUT = 0`
- all `mipi_tx_*_LP_*_OE = 0`
- all `mipi_tx_*_LP_*_OUT = 0`
- all `mipi_tx_*_RST = 1`

Files such as `dsi_tx_top.v`, `panel_config.v`, `reset_ctrl.v`, and vendor `dsi_tx` IP remain in the project, but no active top-level DSI output path is instantiated.

### 5.9 HDMI CDC and 2-Pixel to 1-Pixel Bridge

Module: `fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

Each channel has one bridge:

| Channel | Instance | Write clock | Read clock |
|---|---|---|---|
| ch0 | `u_hdmi0_video_cdc` | `i_sysclk_div2` | `hdmi_tx_slow_clk` |
| ch1 | `u_hdmi1_video_cdc` | `i_sysclk_div2` | `hdmi_tx_slow_clk` |

Parameters:

| Parameter | Value |
|---|---:|
| `FIFO_DEPTH` | `1024` |
| `START_LEVEL` | `16` |
| FIFO word width | `51` |

FIFO payload:

```verilog
{i_vs, i_hs, i_de, i_data[47:0]}
```

Input:

| Signal | Width | Domain |
|---|---:|---|
| `i_vs/i_hs/i_de` | `1` | `i_sysclk_div2` |
| `i_data` | `48` | `i_sysclk_div2` |

Output:

| Signal | Width | Domain |
|---|---:|---|
| `o_vs/o_hs/o_de` | `1` | `hdmi_tx_slow_clk` |
| `o_data` | `24` | `hdmi_tx_slow_clk` |
| `o_active` | `1` | `hdmi_tx_slow_clk` |
| `o_underflow` | `1` | `hdmi_tx_slow_clk` |

Phase sequence:

```text
PH_REQUEST
  -> request next 51b word
PH_LOW
  -> output fifo_q[23:0]
PH_HIGH
  -> output active_word[47:24]
```

Important detail:

- The bridge outputs the low half first, then high half.
- Given the top-level packing `{odd RGB, even RGB}`, this means even pixel first, then odd pixel.
- If image pairs appear horizontally swapped, check this bridge first.

Another important detail:

- FIFO write enable is hardwired to `1'b1`.
- The bridge writes every `i_sysclk_div2` cycle, including blanking cycles.
- Blanking data is later forced to zero when `de=0`, but FIFO bandwidth/level still includes blanking words.

### 5.10 HDMI Top and TMDS Output

Module: `fpga/rtl/dvi_tx/hdmi_top.v`

`hdmi_top` has two possible sources:

1. live selected camera video,
2. internal `color_bar_rgb` fallback pattern.

Fallback pattern:

```verilog
TEST_MODE = 2'd2
DYN_EN = 1'b0
MAX_HRES = 1920
MAX_VRES = 1080
```

The live input is used only when:

```verilog
use_input_video = video_path_ready &
                  (!USE_INPUT_STABLE_GATE || input_stable_seen)
```

Current top-level sets:

```verilog
USE_INPUT_STABLE_GATE = 1'b1
```

Input stable requirements:

- `i_video_ready` is true.
- `vid_info_det` says frame timing is stable.
- active size is exactly `1920 x 1080`.
- active/total/sync error flags are clear.
- `INPUT_STABLE_FRAME_COUNT = 4` good frames have passed.

If HDMI shows color bars:

- TMDS path is alive.
- Live camera path has not yet passed `hdmi_top` gating.
- Check `selected_frame_ready`, underflow flags, bridge active, `hdmi_video_ready`, and `hdmi_input_stable`.

TMDS encoding:

- `dvi_encoder` maps blue to `tmds_data0`, green to `tmds_data1`, red to `tmds_data2`.
- Hsync/vsync control tokens are carried through the blue channel.
- TMDS clock is the constant pattern `10'b1111100000`.
- TX output enables are tied high and resets are tied low in `hdmi_top`.

## 6. Included Modules by Category

### 6.1 Active or Directly Supporting Current Video Path

| Area | Modules/files |
|---|---|
| Top | `top.v` |
| CSI wrapper | `soft_mipi_rx_top.v` |
| Vendor CSI IP | `ip_vendor/csi_rx_controller/csi_rx_controller.sv` |
| Camera I2C | `i2c_master_ctrl_top.v`, `i2c_master_reg_rom.v`, `i2c_master_reg_set.v`, `i2c_16addr_8data.v`, `i2c_8addr_8data.v`, `oc_i2c_master.v`, `i2c_master_top.v`, `i2c_master_bit_ctrl.v`, `i2c_master_byte_ctrl.v` |
| Framebuffer | `frame_buffer.v`, `vid_rx_align_v1.v`, `ser2par_24_128_v1.v`, `ser2par.v`, `ddr_buffer.v`, `ddr_wr_buffer.v`, `ddr_rd_buffer.v`, `bank_switch.v`, `fifo_d512t128.v`, `par2ser_512t128.v`, `par2ser_parse.v`, `data_tx.v`, `vid_par2ser.v`, `rst_n_piple.v`, `elt_dcfifo_v10.v` |
| AXI | `axi_interconnect.v`, `arbiter.v`, `priority_encoder.v` |
| Debayer | `debayer_top_2to1.v`, `line_buffer.v`, `raw_to_rgb.v`, `rgb_gain.v` |
| HDMI | `video_2pix_to_1pix_cdc.v`, `hdmi_top.v`, `dvi_encoder.v`, `encode.v`, `vid_info_det_v7.v`, `color_bar_rgb.v` |
| White balance | `white_balance.v`, instantiated but bypassed |

### 6.2 Present but Not Active in Current Top-Level Behavior

| Area | Status |
|---|---|
| DSI | `dsi_tx_top.v`, `panel_config.v`, `reset_ctrl.v`, vendor `dsi_tx` IP are present, but top-level DSI outputs are tied off |
| Contrast/brightness | `contrast_bright_top.v`, `Contrast_Adj.v`, `Reciprocal.v` are present, but macro is disabled |
| UVC/YUV | `uvc_top.v`, `rgb_to_ycbcr.v`, `yuv444_yuv422.v` are present, but macro is disabled |
| Alternate CSI top | `mipi_csi_top.sv` is present, but current top instantiates `soft_mipi_rx_top` directly |
| Alternate AXI mux | `Axi_Mux.v` is present, but current top uses `axi_interconnect` |
| Checker/debug | `color_bar_checker.v` is present, but top-level instances are commented out |

## 7. CPU-Side Code Status

CPU code is under `cpu/app`.

Current `main.c` behavior:

```text
bsp_init()
print "TJ375 final decision app skeleton"
print next-check hint
loop forever:
  delay 1 second
  print "."
```

Current CPU modules:

| File | Current implementation |
|---|---|
| `main.c` | UART skeleton loop |
| `board_io.c` | `board_io_read_feature_word()` always returns `0`; write is no-op |
| `vision_classifier.c` | returns `{0,0,0,0}` |
| `task_matcher.c` | always returns `0`, meaning no grab |
| `arm_controller.c` | initializes simple `arm_id/state` struct |
| `mycobot_protocol.c` | builds a minimal frame header, no full command pipeline |
| `param_table.c` | returns version `1` |
| `conveyor_control.c` | no-op |

`cpu/app/include/bsp.h` is explicitly provisional:

- UART base addresses are placeholders.
- CLINT/PLIC/APB addresses are placeholders.
- The file says final values must come from the generated Efinity `soc.h`.

Therefore the CPU is not currently participating in the FPGA video chain, classification, or robot control.

## 8. Integration Documents vs Current RTL

`integration/video_pipeline.md` describes the intended long-term pipeline:

```text
video_in -> raw_unpack -> debayer -> wb_gamma -> roi_crop -> feature_extract -> osd -> dvi_tx
```

Current RTL implements only part of that:

| Intended stage | Current status |
|---|---|
| `video_in` | implemented through MIPI CSI wrapper |
| `raw_unpack` | partially implemented as RAW10 to RAW8 truncation |
| `debayer` | implemented |
| `wb_gamma` | white_balance exists but is bypassed; gamma not seen in active chain |
| `roi_crop` | not implemented in active RTL |
| `feature_extract` | not implemented in active RTL |
| `osd` | not implemented in active RTL |
| `dvi_tx` | implemented through `hdmi_top`/`dvi_encoder` |

`integration/register_map.md` and `integration/fpga_cpu_interface.md` define a planned CPU/FPGA register interface:

- `REG_MAGIC`
- `REG_VERSION`
- `REG_FRAME_ID`
- `REG_FEATURE_VALID`
- `REG_FEATURE_ACK`
- ROI/bbox/statistic registers
- CPU result registers
- task/arm/debug registers

Current RTL top does not implement this register map, and current CPU code does not use it.

## 9. Current Implementation Contracts

For debugging and future changes, treat the current system as four contracts.

### 9.1 CSI Contract

Each camera must produce:

```text
rx_out_vs
rx_out_hs
rx_out_de
rx_out_data[39:0]
rx_out_datatype = 6'h2B
rx_out_pixel_per_clk = 4
```

All in the `i_sysclk_div2` pixel domain.

### 9.2 Framebuffer Contract

Each channel must:

```text
accept 32b = 4 x RAW8 per i_sysclk_div2 beat
write stable frames to DDR through AXI0
read frames back from DDR through AXI0
regenerate 960 x 1080 timing in i_sysclk_div2
output 16b = 2 x RAW8 per beat
assert ch*_frame_ready
avoid ch*_fifo_underflow
```

### 9.3 Debayer Contract

Each channel must:

```text
accept 16b = 2 x RAW8 per i_sysclk_div2 beat
apply assumed Bayer pattern in raw_to_rgb
output 48b = 2 x RGB888 per beat
preserve vs/hs/de timing
```

Current gain/white balance caveat:

- `rgb_gain` output is not used by `line_buffer`.
- white balance output is bypassed before HDMI.

### 9.4 HDMI Contract

The HDMI bridge and top must:

```text
cross 48b dual-pixel video from 70 MHz to 140 MHz
emit 24b RGB pixels
turn 960 x 1080 dual-pixel timing into 1920 x 1080 one-pixel timing
avoid bridge underflow
pass hdmi_top stable timing gate for 4 frames
drive TMDS output
```

If output remains color bars, the failure is upstream of `use_input_video`, not necessarily in TMDS.

## 10. Watch Items and Risks

1. RAW10 precision is truncated to RAW8 before DDR and debayer.

2. `ch*_g/ch*_b` are misleading names before debayer; they are raw Bayer bytes, not color channels.

3. `rgb_gain` is instantiated but not in the effective data path.

4. `white_balance` is instantiated but bypassed by constant `HDMI_BYPASS_WHITE_BALANCE = 1'b1`.

5. `video_2pix_to_1pix_cdc` outputs low half first, then high half. This may reverse every pair of pixels depending on upstream packing expectations.

6. `video_2pix_to_1pix_cdc` writes blanking cycles into FIFO because `WrEn = 1'b1`.

7. DSI files remain in the project and constraints, but top-level DSI output is inactive.

8. CPU register map and control loop are design intent, not implemented hardware/software behavior yet.

9. The SW4 channel-toggle debounce implementation may not actually wait for the counter before accepting a changed value.

10. `constrain.sdc` still includes many DSI/MIPI-TX constraints even though DSI is tied off. This can produce confusing warnings and should be interpreted in light of the current top-level tie-off.

## 11. Practical Debug Order

Use this order for board bring-up:

1. Reset and DDR:
   - `sys_pll_lock`
   - `ddr_pll_lock`
   - `pll_byteclk_locked`
   - `arst_n`
   - `rst_n`
   - `ddr_cfg_ok`
   - `pixel_data_en`

2. CSI:
   - `rx_out_vs/hs/de`
   - `rx_out_data[39:0]`
   - `rx_out_datatype == 6'h2B`
   - `rx_out_pixel_per_clk == 4`

3. Framebuffer:
   - `frame_start`
   - `frame_stable`
   - `ddr_frame_len`
   - `wr_frame_done`
   - `rd_start`
   - `ch*_frame_ready`
   - `ch*_fifo_underflow`

4. Debayer:
   - `ch*_vs/hs/de`
   - `ch*_bayer_2pix`
   - `rgb*_vs/hs/de`
   - `rgb*_datax2`

5. HDMI CDC:
   - `hdmi*_bridge_active`
   - `hdmi*_bridge_underflow`
   - `hdmi*_bridge_vs/hs/de/data`

6. HDMI gating:
   - `selected_frame_ready`
   - `selected_fifo_underflow`
   - `selected_bridge_active`
   - `selected_bridge_underflow`
   - `hdmi_video_ready`
   - `hdmi_input_stable`
   - `led[3]`

## 12. Bottom Line

Current `final_project` implements a dual-camera FPGA video display path, not the full contest robot decision system.

Implemented now:

- dual MIPI CSI receive,
- camera I2C init wrapper,
- RAW10 to RAW8 conversion,
- dual DDR framebuffer,
- AXI0 arbitration for two video channels,
- Bayer to RGB debayer,
- HDMI fallback color bars,
- dual-channel HDMI CDC,
- SW4-selected live HDMI path,
- TMDS output.

Not implemented or not active now:

- CPU vision feature readout,
- CPU classifier and task matcher,
- FPGA feature extraction,
- ROI crop,
- OSD,
- CPU/FPGA register map,
- myCobot closed-loop control,
- UVC/YUV path,
- contrast/brightness adjustment,
- DSI display output,
- effective white balance/gain in the active HDMI path.
