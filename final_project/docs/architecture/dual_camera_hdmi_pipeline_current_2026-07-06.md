# Dual-Camera CSI to HDMI Current Architecture

Date: 2026-07-06

Scope: current `final_project/fpga` RTL, as read from the working tree. This is a debug-oriented summary of the existing dual-camera path to HDMI. It does not describe the older DSI display path except where it affects reset or tie-off behavior.

## 1. Top-Level Build and Active Macros

The active top is `final_project/fpga/rtl/top/top.v`, included by `final_project/fpga/efinity/mem_test.xml`.

Active compile-time switches in `top.v`:

```verilog
`define FRAME_BUFFER
`define HDMI_OUT_EN
```

Inactive paths:

- `CONTRAST_BRIGHT_EN` is disabled.
- `UVC_EN` is disabled.
- MIPI DSI TX is no longer part of the active display path. DSI/LCD power and reset ports are tied off, and MIPI TX lane outputs are driven to safe inactive values.

Current visible chain:

```text
S0 MIPI CSI lanes -> soft_mipi_rx_top -> rx_out_*  -> frame_buffer ch0 -> debayer ch0 -> optional white_balance ch0 -> video_2pix_to_1pix_cdc ch0
                                                                                                                               \
                                                                                                                                -> SW4 channel select -> hdmi_top -> dvi_encoder -> TMDS
                                                                                                                               /
S1 MIPI CSI lanes -> soft_mipi_rx_top -> rx_out_*1 -> frame_buffer ch1 -> debayer ch1 -> optional white_balance ch1 -> video_2pix_to_1pix_cdc ch1
```

Current HDMI source selection:

- `channel_sel = 0`: HDMI shows camera S0 / channel 0.
- `channel_sel = 1`: HDMI shows camera S1 / channel 1.
- `i_sw[1]` is treated as an active-low touch button and toggles `channel_sel`.
- `led[2] = channel_sel`.
- `led[3] = hdmi_input_stable`, meaning `hdmi_top` has accepted selected input timing, not merely that a framebuffer has data.

## 2. Clock and Reset Domains

### Clocks from constraints/peripheral XML

Observed clock constraints:

| Signal | Approx freq | Role |
|---|---:|---|
| `mipi_clk` | 100 MHz | CSI controller AXI/config and camera I2C control clock |
| `mipi_rx_ck0_CLKOUT` / `mipi_rx_ck1_CLKOUT` | 100 MHz constrained | CSI D-PHY byte clocks into each CSI controller wrapper |
| `i_sysclk_div2` | 70 MHz | CSI pixel clock, framebuffer input/output pixel domain, debayer/white_balance write side |
| `axi0_ACLK` | 200 MHz | LPDDR/AXI master clock for both framebuffers |
| `i_fb_clk` | 25 MHz | reset delay and DDR config state machine |
| `hdmi_tx_slow_clk` | 140 MHz | HDMI pixel/TMDS encoder slow clock and final 1-pixel-per-clock domain |
| `hdmi_tx_fast_clk` | 700 MHz | LVDS/TMDS serializer fast clock from peripheral block |

Important CDC boundaries:

| Boundary | Mechanism |
|---|---|
| CSI byte clock -> CSI pixel clock | handled inside `csi_rx_controller` IP |
| CSI/framebuffer write pixel domain `i_sysclk_div2` -> AXI `axi0_ACLK` | write-side DC FIFO in `ddr_wr_buffer` |
| AXI `axi0_ACLK` -> framebuffer output `i_sysclk_div2` | read-side DC FIFO in `fifo_d512t128` |
| 2-pixel video `i_sysclk_div2` -> 1-pixel HDMI `hdmi_tx_slow_clk` | `video_2pix_to_1pix_cdc` DC FIFO |

### Reset/ready chain

Top-level reset flow:

```text
i_sw[0] releases PLL resets
sys_pll_lock & ddr_pll_lock & pll_byteclk_locked -> arst_n
arst_n delayed in i_fb_clk -> rst_n
DDR config FSM -> ddr_cfg_ok
ddr_cfg_ok -> sys_rst_n
sys_rst_n delayed in i_sysclk_div2 -> pixel_data_en
```

Current `arst_n` explicitly does not depend on `MIPI_TX_PLL_LOCKED`, because DSI TX is decommissioned:

```verilog
assign arst_n = sys_pll_lock & ddr_pll_lock & pll_byteclk_locked;
```

`pixel_data_en` is the main video-system release signal. It resets/releases:

- both `frame_buffer` instances,
- both AXI interconnect instances,
- debayer and white_balance,
- both `video_2pix_to_1pix_cdc` bridges.

Debug implication: if HDMI remains on fallback color bars, first verify `ddr_cfg_ok`, then `pixel_data_en`, then selected channel `frame_ready`, bridge `o_active`, and finally `hdmi_input_stable`.

## 3. CSI Receive Stage

Module: `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

Each camera uses one `soft_mipi_rx_top` instance:

- S0 maps to `mipi_rx_ck0_*`, `mipi_rx_dp00..03_*`, `S0_io_cam_*`.
- S1 maps the same wrapper ports onto `mipi_rx_ck1_*`, `mipi_rx_dp10..13_*`, `S1_io_cam_*`.

The wrapper instantiates vendor IP `csi_rx_controller`.

Relevant IP settings from `ip_vendor/csi_rx_controller/settings.json`:

- `NUM_DATA_LANE = 4`
- `HS_BYTECLK_MHZ = 70`
- `CLOCK_FREQ_MHZ = 100`
- `DPHY_CLOCK_MODE = "Continuous"`
- `PIXEL_FIFO_DEPTH = 4096`
- `Pack_40/48/56/64` are enabled

Wrapper output:

| Signal | Width | Clock domain | Meaning |
|---|---:|---|---|
| `rx_out_vs` / `rx_out_vs1` | 1 | `i_sysclk_div2` | CSI frame sync, VC0 |
| `rx_out_hs` / `rx_out_hs1` | 1 | `i_sysclk_div2` | CSI line sync, VC0 |
| `rx_out_de` / `rx_out_de1` | 1 | `i_sysclk_div2` | `pixel_data_valid` from CSI IP |
| `rx_out_data` / `rx_out_data1` | 40 | `i_sysclk_div2` | low 40 bits of IP `pixel_data[63:0]` |

Top-level RAW extraction:

```verilog
vin = {
    rx_out_data[39:32],
    rx_out_data[29:22],
    rx_out_data[19:12],
    rx_out_data[9:2]
};
```

This drops two LSBs from each RAW10 sample and converts each 10-bit sample to 8-bit. The resulting `vin` is four RAW8 samples per `i_sysclk_div2` cycle, 32 bits total.

## 4. Framebuffer Stage

Module: `final_project/fpga/rtl/framebuffer/frame_buffer.v`

There are two independent framebuffer instances:

| Channel | Instance | Start address |
|---|---|---:|
| ch0/S0 | `u_frame_buffer` | `33'h0000_0000` |
| ch1/S1 | `u_frame_buffer1` | `33'h0_0180_0000` |

Both are configured with:

- `AXI_DATA_WIDTH = 512`
- `I_VID_WIDTH = 32`
- `O_VID_WIDTH = 16`
- `FB_NUM = 3`
- `BURST_LEN = 63`
- active geometry source output timing: `HACT/2` by `VACT`, currently `960 x 1080` at `i_sysclk_div2`.

### Input packing to DDR

`vid_rx_align_v1` receives:

```text
i_clk = i_sysclk_div2
i_vs/i_hs/i_de = rx_out_vs/rx_out_hs/rx_out_de
vin[31:0] = 4 x RAW8 samples
```

Inside `vid_rx_align_v1`:

```text
32-bit RAW group -> ser2par_24_128_v1 configured 32 -> 64
64-bit groups -> ser2par configured 64 -> 512
512-bit words -> write FIFO -> AXI write bursts
```

Despite the historical filename `ser2par_24_128_v1`, the current parameterization is `I_VID_WIDTH = 32`, `O_VID_WIDTH = 64`.

Frame length is derived in `frame_info_det`:

```text
total_frame_bytes = frame_pix_num * (I_VID_WIDTH / 8)
ddr_frame_len = ceil(total_frame_bytes / 64 bytes)
```

For a stable 1920x1080 RAW8-equivalent frame represented as 4 pixels per 32-bit input beat:

- `frame_pix_num` counts input beats, not individual Bayer pixels.
- expected beats: `1920*1080/4 = 518400`.
- total bytes: `518400 * 4 = 2,073,600`.
- 512-bit AXI words: `2,073,600 / 64 = 32,400`.

### DDR/AXI arbitration

Both framebuffers share external AXI port 0 through two `axi_interconnect` instances:

- `uw_axi_interconnect`: write channels from both framebuffers to `axi0_AW*`/`axi0_W*`/`axi0_B*`.
- `ur_axi_interconnect`: read channels from both framebuffers to `axi0_AR*`/`axi0_R*`.

External AXI port 1 is currently tied off in `top.v`.

The AXI ID width in top is now `AXI_ID_WIDTH = 6`, matching the declared top AXI ports.

### Readback and output timing

The framebuffer does not pass through original camera timing to HDMI. It regenerates output timing with `data_tx`.

Current output timing passed from `top.v` into both framebuffers:

| Signal | Value |
|---|---:|
| `H_FRONT_PORCH` | `HFP >> 1 = 60` |
| `H_SYNC` | `HSP >> 1 = 2` |
| `H_VALID` | `HACT >> 1 = 960` |
| `H_BACK_PORCH` | `HBP >> 1 = 44` |
| `V_FRONT_PORCH` | `20` |
| `V_SYNC` | `2` |
| `V_VALID` | `1080` |
| `V_BACK_PORCH` | `20` |

Readback path:

```text
AXI read 512-bit -> fifo_d512t128 -> 128-bit chunks -> par2ser_parse/vid_par2ser -> 16-bit RAW pair -> data_tx timing generator
```

Framebuffer output to top:

| Signal | Width | Domain | Meaning |
|---|---:|---|---|
| `ch*_vs` | 1 | `i_sysclk_div2` | regenerated frame sync |
| `ch*_hs` | 1 | `i_sysclk_div2` | regenerated line sync |
| `ch*_de` | 1 | `i_sysclk_div2` | regenerated active video |
| `{ch*_g, ch*_b}` | 16 | `i_sysclk_div2` | two RAW8 samples per clock, not RGB |
| `ch*_frame_ready` | 1 | `i_sysclk_div2`, later synchronized to HDMI | framebuffer has a stable written frame and read timing is enabled |
| `ch*_fifo_underflow` | 1 | `i_sysclk_div2`, later synchronized to HDMI | framebuffer output FIFO underflow latched for current frame |

Naming warning: `ch*_g` and `ch*_b` are only 8-bit containers. At this point they are still Bayer/RAW samples, not green/blue color channels.

## 5. Debayer Stage

Module: `final_project/fpga/rtl/debayer/debayer_top_2to1.v`

Both channels instantiate `debayer_top_2to1` in `i_sysclk_div2` domain.

Input:

```verilog
raw_datax4_i = {ch*_b, ch*_g};  // 16 bits = two RAW8 samples
raw_valid_i = ch*_de;
```

The module name/comments are misleading: at the current top-level connection it processes two RAW8 samples per clock, not four RGB pixels.

Internal stages:

```text
optional rgb_gain instance exists, but line_buffer currently receives raw_datax4_i directly
line_buffer builds 3-line context with 16-bit words
raw_to_rgb interpolates Bayer to two RGB pixels per clock
```

Output packing from `debayer_top_2to1`:

```verilog
rgb_datax2_o = {
    b_odd[7:0], g_odd[7:0], r_odd[7:0],
    b_even[7:0], g_even[7:0], r_even[7:0]
};
```

Top-level then converts BGR packing to RGB packing for HDMI/white_balance:

```verilog
rgb*_data_rgb = {
    r_odd, g_odd, b_odd,
    r_even, g_even, b_even
};
```

Debug implication for wrong colors:

1. Confirm RAW10-to-RAW8 bit extraction from CSI.
2. Confirm Bayer order assumed by `raw_to_rgb` matches SC431HAI output.
3. Confirm BGR-to-RGB repack in top.
4. Only after that investigate white balance.

## 6. White Balance Stage

Module: `final_project/fpga/rtl/uvc_src/white_balance.v`

Two white_balance instances exist and are connected, but HDMI currently bypasses them:

```verilog
localparam HDMI_BYPASS_WHITE_BALANCE = 1'b1;
```

So the active HDMI data is:

```text
debayer RGB -> rgb*_data_rgb -> hdmi*_data_out
```

White_balance behavior if re-enabled:

- Input/output width is 48 bits: two RGB888 pixels per clock.
- It accumulates R/G/B averages over `de_in`.
- It updates red and blue Q8.8 gains at `vs_fall`.
- Green remains unchanged.

## 7. HDMI CDC and 2-Pixel to 1-Pixel Conversion

Module: `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

Purpose:

- Accept 48-bit two-pixel RGB data in `i_sysclk_div2`.
- Cross into `hdmi_tx_slow_clk`.
- Output 24-bit one-pixel RGB data.

Interface:

| Signal | Width | Domain | Meaning |
|---|---:|---|---|
| `i_vs/i_hs/i_de` | 1 | `wr_clk = i_sysclk_div2` | two-pixel video timing |
| `i_data` | 48 | `wr_clk` | `{odd RGB, even RGB}` |
| `o_vs/o_hs/o_de` | 1 | `rd_clk = hdmi_tx_slow_clk` | one-pixel HDMI timing |
| `o_data` | 24 | `rd_clk` | RGB888 one pixel |
| `o_active` | 1 | `rd_clk` | FIFO has reached start threshold and bridge is running |
| `o_underflow` | 1 | `rd_clk` | bridge ran out of FIFO data |

Implementation details:

- Internal FIFO word width is 51 bits: `{vs, hs, de, data[47:0]}`.
- FIFO starts output after `fifo_rd_usedw >= START_LEVEL`, currently 16.
- Output phase sequence:
  - `PH_REQUEST`: fetch next 51-bit word.
  - `PH_LOW`: output `fifo_q[23:0]`, the even pixel.
  - `PH_HIGH`: output `active_word[47:24]`, the odd pixel.

This means the active output pixel order is currently even pixel first, then odd pixel.

Important note: `WrEn` is hardwired to `1'b1`, so the bridge writes every `i_sysclk_div2` clock, including blanking cycles. Blanking timing words have `de=0`; data is ignored downstream, but they still consume FIFO bandwidth.

## 8. HDMI TX Stage

Module: `final_project/fpga/rtl/dvi_tx/hdmi_top.v`

Input from top:

```text
i_hs/i_vs/i_de/i_rdata/i_gdata/i_bdata in hdmi_tx_slow_clk domain
i_video_ready from selected framebuffer/bridge gating
```

`hdmi_top` contains two video sources:

- Input camera video.
- Internal `color_bar_rgb` fallback test pattern.

The active source is selected by:

```verilog
use_input_video = video_path_ready & (!USE_INPUT_STABLE_GATE || input_stable_seen);
video_path_ready = sys_rst_n & i_video_ready;
```

With current `USE_INPUT_STABLE_GATE = 1`, `hdmi_top` keeps showing fallback color bars until selected input timing qualifies for several frames.

Input timing qualification:

- `vid_info_det` runs in `hdmi_tx_slow_clk`.
- Active size must be exactly `MAX_HRES x MAX_VRES`, currently `1920 x 1080`.
- Active, total, and hsync error flags must be clear.
- `INPUT_STABLE_FRAME_COUNT = 4`.

Debug implication: if the monitor shows color bars, TMDS output is alive but `use_input_video` is still false. Check:

```text
selected_frame_ready
selected_fifo_underflow
selected_bridge_active
selected_bridge_underflow
hdmi_video_ready
input_h_act / input_v_act
input_*_error
hdmi_input_stable
```

Potential timing-size concern: upstream framebuffer generates `960 x 1080` in the two-pixel domain, then `video_2pix_to_1pix_cdc` expands it to one-pixel clocking. If the bridge emits exactly two pixels for each upstream active beat and preserves DE for both phases, `hdmi_top` should measure `1920 x 1080`. If it measures `960 x 1080`, the problem is in the bridge phase/read timing or DE propagation.

TMDS encoding:

- `dvi_encoder` maps blue to TMDS data0, green to data1, red to data2.
- Hsync/vsync control tokens are carried on the blue channel, as normal DVI/HDMI TMDS.
- TMDS clock pattern is constant `10'b1111100000`.

## 9. Current Debug Landmarks

### LED meanings

| LED bit | Meaning |
|---|---|
| `led[0]` | `~ddr_cfg_ok`; on means DDR config not done |
| `led[1]` | `vs_cnt[5]`; toggles/divides S0 CSI VS activity |
| `led[2]` | `channel_sel`; selected HDMI camera channel |
| `led[3]` | `hdmi_input_stable`; selected input accepted by `hdmi_top` |

### Recommended signal checkpoints

Bring-up from source to sink:

1. PLL/reset:
   - `sys_pll_lock`, `ddr_pll_lock`, `pll_byteclk_locked`
   - `arst_n`, `rst_n`, `ddr_cfg_ok`, `pixel_data_en`
2. CSI S0/S1:
   - `rx_out_vs`, `rx_out_hs`, `rx_out_de`, `rx_out_data[39:0]`
   - `rx_out_vs1`, `rx_out_hs1`, `rx_out_de1`, `rx_out_data1[39:0]`
3. Framebuffer write/read:
   - `frame_start`, `frame_stable`, `ddr_frame_len`
   - `wr_frame_done`, `rd_start`
   - `ch*_frame_ready`, `ch*_fifo_underflow`
4. Debayer:
   - `ch*_vs/ch*_hs/ch*_de`, `{ch*_b,ch*_g}`
   - `rgb*_vs/rgb*_hs/rgb*_de`, `rgb*_datax2`
5. HDMI CDC:
   - `hdmi*_bridge_active`, `hdmi*_bridge_underflow`
   - `hdmi*_bridge_vs/hs/de/data`
6. HDMI qualification:
   - `hdmi_video_ready`
   - `hdmi_input_stable`
   - `input_h_act`, `input_v_act`
   - `input_h_active_error`, `input_v_active_error`, `input_v_total_error`, `input_h_total_error`, `input_h_sync_error`

## 10. Known Risks and Watch Items

1. RAW10 truncation is intentional in current top-level wiring:
   - only `[9:2]` bits of each 10-bit sample are used.
   - if image is too dark/low contrast, this truncation and sensor gain/exposure should be checked.

2. `ch*_g/ch*_b` names are misleading:
   - they are RAW byte containers before debayer.
   - do not debug them as RGB channels.

3. `video_2pix_to_1pix_cdc` outputs even pixel first, then odd pixel:
   - top-level packing says `rgb*_data_rgb = {odd RGB, even RGB}`.
   - the bridge emits `[23:0]` first, then `[47:24]`.
   - if horizontal pairing looks swapped or every two pixels are reversed, check this first.

4. The HDMI bridge writes blanking words continuously:
   - `WrEn = 1'b1`.
   - if FIFO level drifts or underflows despite nominal 70 MHz -> 140 MHz conversion, inspect blanking ratio and FIFO read phase.

5. HDMI fallback can hide upstream failure:
   - color bars mean HDMI TX works, not necessarily camera works.
   - `led[3]`/`o_input_stable` is the key transition indicator.

6. DSI TX is tied off:
   - `MIPI_TX_PLL_LOCKED` no longer gates `arst_n`.
   - DSI/LCD warnings should be interpreted in light of this decommissioned path, but Efinity constraint warnings still need review if they appear.

7. Output timing is regenerated:
   - camera input timing and HDMI output timing are decoupled by DDR.
   - downstream HDMI debug should use framebuffer/data_tx/bridge timings, not assume CSI hsync/vsync reach HDMI unchanged.

8. CSI byte-clock configuration should be checked against timing constraints:
   - `ip_vendor/csi_rx_controller/settings.json` sets `HS_BYTECLK_MHZ = 70`.
   - `constrain.sdc` currently creates `mipi_rx_ck0_CLKOUT` and `mipi_rx_ck1_CLKOUT` at 10.000 ns, i.e. 100 MHz.
   - this may reflect how the vendor D-PHY exposes/constraints its byte clock, but if CSI capture is unstable, validate the generated IP clocking reports before treating these constraints as harmless.

## 11. Minimal Mental Model

For debug, treat the design as four contracts:

1. CSI contract:
   - each camera must produce stable `40b` RAW packed data and `vs/hs/de` in `i_sysclk_div2`.

2. Framebuffer contract:
   - each channel must store one stable frame to DDR and regenerate a `960x1080` two-pixel stream in `i_sysclk_div2`.

3. HDMI bridge contract:
   - each `960x1080` two-pixel stream must become a `1920x1080` one-pixel stream in `hdmi_tx_slow_clk`, with no underflow.

4. HDMI qualification contract:
   - `hdmi_top` must observe four stable `1920x1080` frames before switching from fallback color bars to live camera video.
