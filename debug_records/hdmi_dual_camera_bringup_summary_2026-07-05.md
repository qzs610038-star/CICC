# HDMI dual-camera bring-up debug summary

- Date: 2026-07-05
- Active build tree: `D:\final_project`
- Workspace mirror: `C:\Users\20306\Desktop\赛题资料\CICC\final_project`
- Goal: show live camera video on HDMI, default S0, keep SW4 channel switching.

## Symptoms observed

1. Efinity syntax error after an intermediate edit:
   - `D:\final_project\fpga\rtl\top\top.v(1490): syntax error near 'hdmi_top'`
   - `D:\final_project\fpga\rtl\top\top.v(1513): syntax error near 'endmodule'`
   - Cause class: malformed `ifdef` / instance context around HDMI top-level wiring.

2. HDMI showed a full-screen color that kept changing, sometimes with white/yellow/blue vertical bars.
   - Diagnosis: this matched `hdmi_top.v` internal `color_bar_rgb(TEST_MODE=2'd1)` fallback, not camera video.
   - `TEST_MODE=2'd1` is dynamic: it cycles full-screen colors and eventually shows vertical bars.

3. HDMI later showed flashing orange/cyan noisy horizontal bands.
   - Diagnosis: HDMI was no longer simply no-signal; it was displaying a generated/unstable video path.
   - Likely contributors: framebuffer readout could generate stable timing while DDR/FIFO data was invalid, and HDMI stability detection only looked at HS/VS/DE.

4. User corrected the LED naming:
   - RTL `led[2]` is the channel select indicator.
   - On the board, the user observes that signal as physical LED20.
   - Do not describe this as board `LED[2]`; say RTL `led[2]` / physical LED20.

## Root causes found

1. HDMI fallback was dynamic and could be mistaken for bad camera video.
   - `hdmi_top.v` used `TEST_MODE=2'd1`, which intentionally changes full-screen colors.

2. HDMI input gating was too late in the pipeline.
   - `vid_info_det` inside `hdmi_top.v` only verifies input timing stability.
   - `data_tx` can generate stable HS/VS/DE even if upstream DDR/FIFO data is invalid.
   - Result: invalid framebuffer data can be treated as a stable HDMI input.

3. Framebuffer read could start before any DDR frame had actually been written.
   - Initial triple-buffer `rd_bank` may point to a bank that has not received a completed frame yet.
   - Result: HDMI may display stale/uninitialized DDR data as colored noise.

4. Frame stability detection in `frame_info_det.v` used the wrong history value.
   - It compared delayed values derived from `frame_pix_num_o`, making stable detection vulnerable to stale frame length.

5. Debayer output order was BGR per module comment, while HDMI expected RGB.
   - `debayer_top_2to1` output comment: `rgb_datax2_o // b,g,r,b,g,r`.
   - Top-level HDMI path needed explicit BGR-to-RGB reorder.

6. `white_balance.v` had a missing `vs_d <= vs_in` update and division-by-zero exposure.
   - HDMI currently bypasses white balance for bring-up, but the module was still cleaned up to avoid bad downstream sync behavior.

## Code changes made

### `fpga/rtl/dvi_tx/hdmi_top.v`

- Added `i_video_ready` input and `o_input_stable` output.
- Added `USE_INPUT_STABLE_GATE`, default `1'b1`.
- HDMI now uses camera input only when:
  - reset is released,
  - selected video path says ready,
  - and `vid_info_det` sees stable input timing.
- Changed fallback test pattern from dynamic `TEST_MODE=2'd1` to static `TEST_MODE=2'd2`.
  - Expected fallback is now stable vertical color bars, not a flashing full-screen color.
- Set fallback timing to 1920x1080-oriented values.

### `fpga/rtl/framebuffer/frame_info_det.v`

- Stable detection now compares actual frame pixel counts across multiple frames.
- Requires three matching non-trivial frame lengths before asserting `frame_stable`.

### `fpga/rtl/framebuffer/ddr_buffer.v`

- Exposed `wr_frame_done`, driven by internal `wr_end_flag`.
- This lets the frame buffer know when at least one DDR frame has really been written.

### `fpga/rtl/framebuffer/frame_buffer.v`

- Added outputs:
  - `frame_ready`
  - `fifo_rd_underflow`
- Added `rd_frame_available_axi` / `rd_frame_available_o`.
  - Readout is not allowed until a DDR write frame completes.
- Gated `rd_start` and `frame_en` with the completed-frame flag.
- Captured `data_tx` `fifo_rd_underflow`.
  - Underflow is latched during a readout frame and cleared at the next frame boundary.
- `frame_ready` now requires:
  - frame stable,
  - out_sync,
  - at least one written DDR frame available,
  - and no latched read FIFO underflow.

### `fpga/rtl/top/top.v`

- Preserved default S0 selection after reset.
- Preserved SW4 channel switching.
- Debounced SW4 in the HDMI clock domain and toggles `channel_sel`.
- Added selected-channel readiness:
  - S0 uses `ch0_frame_ready` / `ch0_fifo_underflow`.
  - S1 uses `ch1_frame_ready` / `ch1_fifo_underflow`.
- HDMI `i_video_ready` now follows the selected channel's frame readiness, not only a delay counter.
- RTL `led[2]` remains channel indicator; user observed physical LED20.
- RTL `led[3]` indicates selected channel has valid framebuffer output and no underflow.
- Bypassed white balance for HDMI bring-up with `HDMI_BYPASS_WHITE_BALANCE = 1'b1`.
- Reordered debayer `{B,G,R,B,G,R}` output to `{R,G,B,R,G,B}` before HDMI/AWB.
- Decommissioned DSI TX path in top-level tie-offs for HDMI-only bring-up.

### `fpga/rtl/uvc_src/white_balance.v`

- Added missing `vs_d <= vs_in`.
- Reset `vs_d`.
- Guarded average divisions against zero.

## Sync status

Changed files were copied from:

`C:\Users\20306\Desktop\赛题资料\CICC\final_project`

to:

`D:\final_project`

Relevant D-drive files:

- `D:\final_project\fpga\rtl\dvi_tx\hdmi_top.v`
- `D:\final_project\fpga\rtl\framebuffer\frame_info_det.v`
- `D:\final_project\fpga\rtl\framebuffer\ddr_buffer.v`
- `D:\final_project\fpga\rtl\framebuffer\frame_buffer.v`
- `D:\final_project\fpga\rtl\top\top.v`
- `D:\final_project\fpga\rtl\uvc_src\white_balance.v`

## Verification run

Command:

```powershell
Set-Location D:\final_project\fpga\efinity
& 'D:\Efinity\2025.2\bin\efx_map.exe' --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_frame_ready_v2 --output-dir work_syn_codex_frame_ready_v2
```

Result:

- Exit code: 0
- `D:\final_project\fpga\efinity\work_syn_codex_frame_ready_v2\EFX.err.log`: no errors, only synthesis start header.
- `EFX.warn.log`: existing warning set remains.
- Synthesis log confirms fallback `color_bar_rgb(... TEST_MODE=2'b10)`.

Remaining warnings to revisit later:

- `ser2par_24_128_v1.v` and `vid_par2ser.v`: divide-by-zero warnings from generic width helper functions.
- `frame_buffer.v`: `i_hs` not connected into `vid_rx_align_v1`; current logic mainly uses VS/DE, but this should be reviewed.
- `frame_buffer.v`: `rd_fifo_rdvalid` has no driver, likely a stale local reg.
- `white_balance.v`: expression size truncation at average calculations; HDMI bypasses white balance now, but this should be cleaned before enabling AWB.
- Many existing unconnected top/IP ports remain from project skeleton and disabled DSI/CPU paths.

## Next board-test interpretation

After rebuilding/flashing `D:\final_project`:

1. If HDMI shows stable vertical color bars and no flashing:
   - HDMI physical path and fallback are working.
   - The selected camera/framebuffer path is still not ready.
   - Check RTL `led[3]`; if off, debug CSI/framebuffer/DDR write completion next.

2. If RTL `led[3]` turns on and HDMI switches away from static color bars:
   - The selected framebuffer path is passing the new readiness gate.
   - If the image is still noisy, continue with RAW packing, Bayer phase, DDR read/write ordering, and 2x-to-1x pixel unpack phase.

3. If flashing full-screen colors still appear:
   - The board is probably running an old bitstream or a different project copy.
   - The current `D:\final_project` fallback should not flash because it uses `TEST_MODE=2'd2`.

4. If physical LED20 toggles with SW4:
   - SW4 channel switching is still alive.
   - LED20 corresponds to RTL `led[2]`, not a complete board LED index map.

5. If switching to S1 changes behavior:
   - Compare S0 and S1 `frame_ready` behavior.
   - Current reset default is S0, per user requirement.

## Recommended next debug target

If static color bars remain and RTL `led[3]` stays off, inspect:

- MIPI CSI output `rx_out_vs/rx_out_de/rx_out_data`.
- `frame_info_det.frame_stable`.
- `ddr_buffer.wr_frame_done`.
- `data_tx.fifo_rd_underflow`.

If RTL `led[3]` turns on but picture remains noisy, inspect:

- `rx_out_data[39:32], [29:22], [19:12], [9:2]` RAW packing.
- `vout({ch*_g,ch*_b})` and debayer `raw_datax4_i({ch*_b,ch*_g})`.
- Bayer phase in `raw_to_rgb`.
- Whether `hdmi_tx_slow_clk` is exactly 2x `i_sysclk_div2` and phase-compatible enough for direct dual-pixel unpacking.

## 2026-07-06 follow-up: HDMI stable gate and 2-pixel CDC bridge

### Board symptom that triggered this pass

The board was still not showing real camera video. The visible HDMI output was:

- full-screen solid color that kept switching;
- sometimes simultaneous white/yellow/blue vertical stripes;
- earlier, the upper half showed changing solid color and the lower half showed horizontal color bands.

This means the monitor and HDMI transmitter are alive, but the HDMI input path is still being fed either by fallback test pattern or by unstable/invalid video data.

### Findings from the code review

1. The fallback pattern itself is not the camera image.
   - `color_bar_rgb(TEST_MODE=2'd1)` produces dynamic full-screen color switching.
   - The current fallback is now `TEST_MODE=2'd2`, a static vertical color-bar pattern.
   - Therefore, if the flashed bitstream still immediately shows full-screen color cycling, first suspect that the board is running an old bitstream or a different project copy.

2. The previous HDMI input release condition was too weak.
   - `hdmi_top.v` could switch from fallback to input once basic HS/VS/DE timing looked stable.
   - That is not enough for this bring-up because the framebuffer can output stable timing while pixel data is stale, underflowed, misordered, or otherwise invalid.

3. The previous 2-pixel-to-1-pixel HDMI handoff was a CDC risk.
   - The camera/framebuffer side produces a 48-bit two-pixel stream in the `i_sysclk_div2` domain.
   - HDMI consumes a 24-bit one-pixel stream in the `hdmi_tx_slow_clk` domain.
   - Directly splitting the 48-bit bus in the HDMI clock domain can create color flicker, phase errors, or stripe-like artifacts if the clocks are not safely phase-aligned.

4. LED naming must be recorded carefully.
   - RTL `led[2]` remains the channel select indicator.
   - The user observed this signal as physical LED20 on the board.
   - Future notes should say "RTL `led[2]` / physical LED20", not assume the board silk number equals the RTL vector index.

### Changes applied

#### `fpga/rtl/dvi_tx/hdmi_top.v`

- Added `INPUT_STABLE_FRAME_COUNT = 4'd4`.
- Kept `USE_INPUT_STABLE_GATE = 1'b1`.
- Tightened HDMI input release so input video is accepted only after:
  - upstream `i_video_ready` is true;
  - `vid_info_det` reports a stable frame;
  - detected active size matches `1920 x 1080`;
  - active/total/sync error flags are clear;
  - four consecutive good frames have passed.
- Added/kept `o_input_stable` so top-level LEDs can show when `hdmi_top` has accepted the selected input path.
- Kept fallback as static color bars with `TEST_MODE=2'd2`.

#### `fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

- Added a new bridge module for the camera/framebuffer to HDMI handoff.
- Uses existing `DC_FIFO` with 51-bit words:
  - `{VS, HS, DE, 48-bit two-pixel RGB data}`.
- Write side:
  - `wr_clk = i_sysclk_div2`;
  - writes one 48-bit two-pixel word every source clock.
- Read side:
  - `rd_clk = hdmi_tx_slow_clk`;
  - emits the low 24-bit pixel first, then the high 24-bit pixel.
- Exposes:
  - `o_active`, indicating the bridge has enough FIFO level to run;
  - `o_underflow`, indicating the HDMI side ran out of bridged video words.

#### `fpga/rtl/top/top.v`

- Instantiated two `video_2pix_to_1pix_cdc` modules:
  - `u_hdmi0_video_cdc` for S0;
  - `u_hdmi1_video_cdc` for S1.
- Moved SW4 selection after the two CDC bridges, so SW4 selects between HDMI-domain 24-bit streams.
- Preserved reset default:
  - `channel_sel <= 1'b0`, so S0 is selected by default.
- Preserved SW4 toggle behavior:
  - `i_sw[1]` is still debounced in the HDMI clock domain and toggles `channel_sel` on the active edge.
- Updated `hdmi_video_ready` so the HDMI input is released only when the selected channel has:
  - framebuffer-ready status;
  - no selected framebuffer underflow;
  - active CDC bridge;
  - no bridge underflow.
- Kept channel debug:
  - `led[2] = channel_sel` for RTL channel select / physical LED20.
- Updated HDMI-accept debug:
  - `led[3] = hdmi_input_stable`, meaning `hdmi_top` accepted the selected input timing after its strict stable gate.

#### `fpga/efinity/mem_test.xml`

- Added the new design file:
  - `../rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

### D-drive sync

Relevant project files were synchronized from:

`C:\Users\20306\Desktop\赛题资料\CICC\final_project`

to:

`D:\final_project`

The D-drive build tree is the active tree for Efinity build/flash.

### Verification run

Command:

```powershell
Set-Location D:\final_project\fpga\efinity
& 'D:\Efinity\2025.2\bin\efx_map.exe' --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_hdmi_cdc_v1 --output-dir work_syn_codex_hdmi_cdc_v1
```

Result:

- Exit code: 0.
- `D:\final_project\fpga\efinity\work_syn_codex_hdmi_cdc_v1\EFX.err.log` has no synthesis errors.
- `EFX.warn.log` still contains the existing project warning set.
- No direct warning from `video_2pix_to_1pix_cdc.v` was found.
- Synthesis log confirms the new `DC_FIFO(DATA_WIDTH=51,FIFO_DEPTH=1024)` bridge and `video_2pix_to_1pix_cdc` module were synthesized.

The C-drive build path was not used as the primary verification path because the Chinese workspace path caused Efinity to fail opening some Verilog files. Use `D:\final_project` for manual Efinity build/flash.

### Next board-test interpretation

After rebuilding/flashing from `D:\final_project`:

1. Stable static vertical color bars, `led[3]` off:
   - HDMI physical path and fallback are working.
   - The selected camera/framebuffer path is still not accepted.
   - Next debug target: CSI output, frame stable detection, DDR write completion, framebuffer underflow.

2. `led[3]` on and live image appears:
   - HDMI input timing and selected channel path are accepted.

3. `led[3]` on but image is noisy, striped, or wrong color:
   - HDMI timing/CDC has passed far enough to display input.
   - Next debug target: RAW10 packing, Bayer phase, RGB channel ordering, DDR read/write ordering.

4. Full-screen color continues to switch immediately:
   - The current fallback should not do that.
   - First suspect an old bitstream, wrong Efinity project, or build/flash from the wrong directory.

5. Physical LED20 toggles when SW4 is pressed:
   - SW4 channel switching is still alive.
   - This confirms physical LED20 corresponds to RTL `led[2]` for this debug purpose.
