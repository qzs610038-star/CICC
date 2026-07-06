# Dual-Camera HDMI Fix Plan

Date: 2026-07-06

Scope: `final_project/fpga` current working tree. This note reviews the external diagnosis against the real RTL and the official demo, then gives a low-risk repair sequence intended to get one HDMI live-camera pass working before deeper image-quality tuning.

## 1. Verdict on the External Diagnosis

| # | External finding | Verdict | Reason |
|---:|---|---|---|
| 1 | `vin` crosses pixel boundaries | Not proven; likely wrong under RAW10 | The official demo uses the same extraction: `[39:32]`, `[29:22]`, `[19:12]`, `[9:2]`. For four packed RAW10 pixels, this is exactly the upper 8 bits of each 10-bit sample. |
| 2 | `PACK_BIT=40` means 5 RAW8 pixels and one pixel is dropped | Not valid for the inherited RAW10 path | In this design, `40b` is expected to mean `4 x RAW10`; `I_VID_WIDTH=32` is the post-truncation `4 x RAW8` stream written to the framebuffer. |
| 3 | `{ch*_g,ch*_b}` then `{ch*_b,ch*_g}` may swap Bayer phase | Real risk, but not an immediate blind fix | This ordering is inherited from the official demo. It should be made switchable and verified on image color/phase, not flipped blindly. |
| 4 | `vout` lacks independent R component | Incorrect interpretation | Before debayer, the stream is Bayer RAW, not RGB. A 16-bit output means two RAW8 pixels per clock. R/G/B are generated later by `debayer_top_2to1`. |
| 5 | HDMI RGB format may mismatch color bar format | Low risk | `hdmi_top` feeds RGB888 into `dvi_encoder`; color bars and input video use the same `i_rdata/i_gdata/i_bdata` path into TMDS encoding. |
| 6 | `video_2pix_to_1pix_cdc` may have first-pixel DE/data phase issue | Worth verifying and hardening | The state machine is plausibly correct for the current `DC_FIFO`, but it ignores `WrFull`/`DataVal` and has no overflow status. This is a better first repair target than changing RAW unpacking. |

## 2. Do Not Apply This Fix Blindly

Do not replace current RAW extraction with either of these unless live CSI status proves the stream is RAW8 byte-packed:

```verilog
// Do not apply as a blind fix for the current RAW10 path.
.vin({rx_out_data[39:32], rx_out_data[31:24], rx_out_data[23:16], rx_out_data[15:8]})
.vin(rx_out_data[31:0])
```

If the source really is RAW10, those changes mix 10-bit pixel fields and will corrupt Bayer phase. The current extraction should instead be treated as:

```verilog
wire [31:0] cam0_raw8x4 = {
    rx_out_data[39:32],  // pixel 3 RAW10[9:2]
    rx_out_data[29:22],  // pixel 2 RAW10[9:2]
    rx_out_data[19:12],  // pixel 1 RAW10[9:2]
    rx_out_data[9:2]     // pixel 0 RAW10[9:2]
};
```

## 3. First-Pass Patch Set

### P0 - Make CSI format observable

Expose the CSI IP's existing status outputs instead of guessing:

- In `soft_mipi_rx_top.v`, add outputs for `datatype[5:0]` and `pixel_per_clk[3:0]`.
- In `top.v`, instantiate per-channel wires such as `rx_out_datatype0`, `rx_out_pixel_per_clk0`, `rx_out_datatype1`, `rx_out_pixel_per_clk1`.
- Capture them with Efinity debugger/ILA during live camera input.

Decision gate:

| Observed status | Interpretation | Action |
|---|---|---|
| `datatype = 0x2b`, `pixel_per_clk = 4` | RAW10, 4 pixels per clock | Keep current RAW10-to-RAW8 extraction. |
| `datatype = 0x2a`, `pixel_per_clk = 5` or equivalent | RAW8 byte stream in 40-bit word | Do not just take low 32 bits; either reconfigure sensor/IP back to RAW10/4ppc, or implement a real 40-bit-to-framebuffer repacker. |
| unstable datatype/ppc | CSI/IP configuration or lane capture is not stable | Debug CSI first; do not tune debayer/HDMI yet. |

### P1 - Replace inline RAW extraction with named wires

This is a no-behavior-change cleanup that prevents future accidental RAW8/RAW10 confusion:

```verilog
wire [31:0] cam0_raw8x4_from_raw10 = {
    rx_out_data[39:32],
    rx_out_data[29:22],
    rx_out_data[19:12],
    rx_out_data[9:2]
};

wire [31:0] cam1_raw8x4_from_raw10 = {
    rx_out_data1[39:32],
    rx_out_data1[29:22],
    rx_out_data1[19:12],
    rx_out_data1[9:2]
};
```

Then connect `.vin(cam*_raw8x4_from_raw10)`.

### P2 - Make Bayer pair order switchable

Keep the official-demo default, but make the only likely color-phase fix a one-line selectable option:

```verilog
localparam BAYER_PAIR_SWAP = 1'b1; // 1 matches current official-demo wiring

wire [15:0] ch0_debayer_raw_pair = BAYER_PAIR_SWAP ? {ch0_b, ch0_g} : {ch0_g, ch0_b};
wire [15:0] ch1_debayer_raw_pair = BAYER_PAIR_SWAP ? {ch1_b, ch1_g} : {ch1_g, ch1_b};
```

Then drive `debayer_top_2to1.raw_datax4_i` from these wires. If live image appears but colors are obviously wrong or checkerboarded, try this switch before editing `raw_to_rgb`.

### P3 - Harden and test `video_2pix_to_1pix_cdc`

Priority checks:

- Add a small testbench that feeds a known `960 x N` two-pixel stream and asserts that output `o_de` has exactly `1920 x N` active cycles.
- Check pixel order: current bridge emits `i_data[23:0]` first, then `i_data[47:24]`.
- Add at least an internal overflow latch or expose `o_overflow`; current `WrEn(1'b1)` ignores `fifo_full`.
- Keep blanking writes unless the timing scheme is redesigned; blanking words are what let the bridge double total horizontal timing from `1066` two-pixel clocks to `2132` HDMI pixel clocks.

Do not change the RAW path until this bridge has passed a deterministic sequence test. If HDMI still shows color bars while CSI/framebuffer are alive, this CDC/timing qualification layer is the most likely blocker.

## 4. Once-Through Bring-Up Order

1. Keep RAW extraction and debayer default behavior unchanged.
2. Add CSI `datatype`/`pixel_per_clk` observability and named RAW unpack wires.
3. Add the Bayer-pair switch with default set to current behavior.
4. Run the CDC testbench for `video_2pix_to_1pix_cdc`.
5. Run Efinity synth/P&R manually from `final_project/fpga/efinity/mem_test.xml`.
6. On board, check:
   - `led[0] == 0`: DDR configured.
   - `led[1]` toggles: S0 VS activity.
   - `led[2]`: selected channel.
   - `led[3] == 1`: `hdmi_top` accepted selected input timing.
7. If HDMI remains color bars:
   - inspect `hdmi_video_ready`, `hdmi_input_stable`, `input_h_act`, `input_v_act`, and `input_*_error`;
   - expected accepted input is `1920 x 1080`, not `960 x 1080`.
8. If live image appears but colors are wrong:
   - first test `BAYER_PAIR_SWAP`;
   - then evaluate `raw_to_rgb` Bayer phase;
   - only then consider white balance.

## 5. Practical Recommendation

For the next code patch, do the smallest safe set:

- expose CSI `datatype` and `pixel_per_clk`;
- replace inline `.vin({...})` with named `raw8x4_from_raw10` wires without changing bit slices;
- add `BAYER_PAIR_SWAP` with the current behavior as default;
- add or run a focused CDC sequence test for `video_2pix_to_1pix_cdc`.

The external RAW8-based fix is a fallback branch, not the main fix. The mainline should stay aligned with the official RAW10 demo until live hardware proves otherwise.
