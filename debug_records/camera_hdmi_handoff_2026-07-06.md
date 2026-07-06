# Dual-Camera HDMI Bring-Up Handoff

- Date: 2026-07-06
- Purpose: migrate the current camera-to-HDMI debug context into a new Codex/agent window without losing the fault history, verified conclusions, and next debug plan.
- Repo root: `C:\Users\20306\Desktop\赛题资料\CICC`
- Formal project tree: `C:\Users\20306\Desktop\赛题资料\CICC\final_project`
- Active build/flash tree confirmed by user: `D:\final_project`
- Efinity install used so far: `D:\Efinity\2025.2`
- Current goal: get live dual-camera video through HDMI, with S0 selected by default and SW4 switching S0/S1.

## 0. New Window Start Instructions

In the new window, read this file first, then verify current files before editing:

```powershell
Set-Location "C:\Users\20306\Desktop\赛题资料\CICC"
git status --short
Get-Item "D:\final_project\fpga\efinity\mem_test.xml"
Get-Item "D:\final_project\fpga\rtl\top\top.v"
Get-Item "D:\final_project\fpga\rtl\dvi_tx\video_2pix_to_1pix_cdc.v"
```

Important operating rule:

- Do not assume the old chat state is current. Check `C:\...\CICC\final_project` and `D:\final_project` before making another RTL change.
- User burns/flashes from `D:\final_project`, not only from the C-drive repo copy.
- Use the official demo as reference, but implement and debug inside `final_project`.
- Manual Efinity build/flash is the current workflow.

## 1. Current Board Symptom

User-provided screenshots/description show HDMI output is alive but live camera is still not through:

- full-screen solid colors that switch;
- sometimes white/yellow/blue vertical bars;
- earlier orange/cyan noisy horizontal bands or split-frame noisy bands.

Interpretation:

- HDMI physical/TMDS output is likely alive.
- Dynamic full-screen color cycling matches the old `hdmi_top` fallback `color_bar_rgb(TEST_MODE=2'd1)`.
- Current intended fallback is static vertical bars with `TEST_MODE=2'd2`.
- If the board still immediately shows full-screen cycling after rebuild/flash, first suspect old bitstream, wrong Efinity project, or flashing from the wrong directory.

## 2. Hard Constraints Known So Far

1. Active project for burning is `D:\final_project`.
2. `final_project` is the formal engineering tree; do not patch the vendor demo as the product codebase.
3. Chinese workspace paths can break Efinity with illegal byte sequence errors. Prefer ASCII build paths such as `D:\final_project` or `D:\cicc_cbm_link\final_project`.
4. Target top is `top` for Efinity Titanium `TJ375N529`.
5. Current active top-level macros are:
   - `FRAME_BUFFER`
   - `HDMI_OUT_EN`
6. DSI/LCD TX path is decommissioned/tied off for this HDMI bring-up. `MIPI_TX_PLL_LOCKED` should not block the main reset release.
7. S0 is default HDMI channel after reset. SW4 toggles channel selection.
8. User corrected LED naming: RTL `led[2]` is observed as physical LED20. Do not call it board LED2 without checking the board map.
9. HDMI is currently gated by selected path readiness and input timing qualification. Static color bars may simply mean upstream path is not accepted yet.
10. The official demo and current intended path are RAW10 based unless live CSI status proves otherwise.

## 3. Current Intended Data Path

```text
S0 MIPI CSI -> soft_mipi_rx_top -> RAW10/4ppc -> RAW8x4 truncation -> frame_buffer ch0
            -> debayer ch0 -> BGR-to-RGB repack -> 2pix-to-1pix HDMI CDC -> hdmi_top -> DVI/HDMI

S1 MIPI CSI -> soft_mipi_rx_top -> RAW10/4ppc -> RAW8x4 truncation -> frame_buffer ch1
            -> debayer ch1 -> BGR-to-RGB repack -> 2pix-to-1pix HDMI CDC -> hdmi_top -> DVI/HDMI

SW4/channel_sel chooses ch0 or ch1 after the HDMI CDC bridges.
```

Clock domains:

| Domain | Signal / clock | Role |
|---|---|---|
| CSI/config | `mipi_clk` | CSI controller config, camera I2C |
| Pixel/framebuffer | `i_sysclk_div2` | CSI pixel output, framebuffer video in/out, debayer, white balance side |
| DDR/AXI | `axi0_ACLK` | LPDDR AXI access |
| HDMI pixel | `hdmi_tx_slow_clk` | one-pixel RGB into HDMI encoder |
| HDMI serialize | `hdmi_tx_fast_clk` | TMDS serializer |

Key CDC boundaries:

- CSI IP handles byte-to-pixel crossing internally.
- Framebuffer write/read uses DC FIFOs around AXI.
- `video_2pix_to_1pix_cdc` crosses `i_sysclk_div2` 48-bit two-pixel RGB into `hdmi_tx_slow_clk` 24-bit one-pixel RGB.

## 4. Faults Already Investigated and Current Verdicts

| ID | Fault / suspicion | Current verdict | Evidence / reasoning | Status |
|---|---|---|---|---|
| F1 | `vin` RAW extraction crosses pixel boundaries because slices are 10-bit spaced | Not a bug if CSI output is RAW10 | Official demo uses `[39:32], [29:22], [19:12], [9:2]`; this is upper 8 bits of four RAW10 samples | Keep current extraction, but verify CSI datatype |
| F2 | `PACK_BIT=40` means 5 RAW8 pixels and `I_VID_WIDTH=32` drops one pixel | Not valid for RAW10 path | In current interpretation, 40b = 4 x RAW10, then 32b = 4 x RAW8 after dropping 2 LSB per pixel | Do not apply RAW8 byte-aligned fix blindly |
| F3 | `{ch*_g,ch*_b}` then `{ch*_b,ch*_g}` may swap Bayer phase | Real risk | Framebuffer `vout` is two RAW bytes, not RGB. Pair order affects Bayer phase | Made switchable, default preserves inherited behavior |
| F4 | Framebuffer output lacks independent R component | Not a bug | Before debayer there is no R/G/B component, only Bayer RAW samples | Closed unless someone misuses names |
| F5 | HDMI input RGB may mismatch color bar RGB | Low immediate risk | `hdmi_top` feeds RGB bytes into `dvi_encoder`; top now repacks debayer BGR to RGB | Verify with color target once live image appears |
| F6 | HDMI dynamic fallback is mistaken for camera failure | Confirmed issue | Old `TEST_MODE=2'd1` intentionally cycles full-screen colors | Current intended fallback is static bars `2'd2` |
| F7 | HDMI switches too early on stable timing but invalid data | Confirmed design risk | `data_tx` can generate stable timing while DDR data is stale/underflowed | Added `i_video_ready`, stricter stable gate |
| F8 | Framebuffer read starts before a DDR frame has been written | Confirmed design risk | Initial read bank can point to empty/uninitialized frame | `frame_ready` now depends on completed write frame |
| F9 | `frame_info_det` stable detection used stale/history value | Confirmed earlier risk | Could assert stable on bad frame length history | Updated to require matching non-trivial frame lengths |
| F10 | Debayer output order is BGR, HDMI expects RGB | Confirmed | `debayer_top_2to1` comment/output order is B,G,R for odd/even | Top-level BGR-to-RGB repack added |
| F11 | `white_balance.v` sync/update issues | Confirmed but bypassed for HDMI | Missing `vs_d <= vs_in` and div-by-zero exposure found | Cleaned, but HDMI currently bypasses WB |
| F12 | 2-pixel to 1-pixel handoff unsafe across clocks | Confirmed high-priority risk | Direct bus split between `i_sysclk_div2` and `hdmi_tx_slow_clk` can create phase/flicker/stripe issues | Added `video_2pix_to_1pix_cdc` bridge |
| F13 | `video_2pix_to_1pix_cdc` first pixel may assert before data valid | Accepted risk from review | Synchronous FIFO read latency must be handled | State machine changed to request then consume |
| F14 | Current board may be running stale bitstream | Still possible | User sees patterns consistent with old dynamic fallback; D-tree sync/build path confusion occurred | Must verify build/flash from `D:\final_project` |
| F15 | Efinity full compile/debug auto-instantiation failure | Not an RTL map failure | One run hit missing `debug_top.v` during debug flow after map | Need normal PNR/bitstream flow or fix debug profile |

## 5. Code Changes Already Made in Working Tree

These are present in the C-drive working tree, with key files also confirmed in `D:\final_project`.

### `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Added diagnostic outputs:
  - `rx_out_datatype[5:0]`
  - `rx_out_pixel_per_clk[3:0]`
- Exported CSI controller `datatype` and `pixel_per_clk`.
- Kept `PACK_BIT=40`.
- `rx_out_data` is low `PACK_BIT` bits of CSI IP `pixel_data[63:0]`.

### `final_project/fpga/rtl/top/top.v`

- Added explicit RAW10-to-RAW8 function:

```verilog
raw10_4pix_to_raw8_4pix = {
    raw10_4pix[39:32],
    raw10_4pix[29:22],
    raw10_4pix[19:12],
    raw10_4pix[9:2]
};
```

- Added CSI format checks:
  - `datatype == 6'h2B`
  - `pixel_per_clk == 4`
- Added Bayer pair switches:
  - `CH0_BAYER_SWAP_PIXELS = 1'b1`
  - `CH1_BAYER_SWAP_PIXELS = 1'b1`
- Default switch value preserves the inherited official-demo order `{ch*_b,ch*_g}`.
- Added two `video_2pix_to_1pix_cdc` bridges.
- Moved channel select after HDMI-domain bridge outputs.
- Kept S0 default and SW4 channel switching.
- HDMI white balance bypass is currently enabled:
  - `HDMI_BYPASS_WHITE_BALANCE = 1'b1`
- `led[2] = channel_sel`.
- Current `led[3] = hdmi_input_stable & selected_csi_format_ok`.

### `final_project/fpga/rtl/dvi_tx/hdmi_top.v`

- Added `i_video_ready`.
- Added `o_input_stable`.
- Fallback pattern changed to `TEST_MODE=2'd2`.
- Input video accepted only after:
  - reset released;
  - selected video path ready;
  - `vid_info_det` reports stable input;
  - active size is `1920 x 1080`;
  - timing error flags clear;
  - four consecutive good frames pass.

### `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

- New bridge module.
- FIFO word is 51 bits: `{vs, hs, de, 48-bit two-pixel RGB}`.
- Write clock: `i_sysclk_div2`.
- Read clock: `hdmi_tx_slow_clk`.
- Current output order:
  - first `i_data[23:0]` / even pixel;
  - then `i_data[47:24]` / odd pixel.
- Exposes `o_active` and `o_underflow`.

### Framebuffer-related files

Earlier edits added/readied:

- `frame_ready`
- `fifo_rd_underflow`
- `wr_frame_done`
- gating so readout waits for at least one completed DDR write frame
- improved frame stability detection

## 6. Verification Already Run

### Map-only verification

One map-only run from ASCII path passed:

```powershell
Set-Location D:\cicc_cbm_link\final_project\fpga\efinity
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && D:\Efinity\2025.2\bin\efx_run.bat mem_test.xml --prj -f map --output_dir work_syn_codex_fix_map_final_ascii --work_dir work_syn_codex_fix_map_final_work_ascii"
```

Result:

- `map : PASS`
- Resource summary recorded in log:
  - `EFX_LUT4`: 12210
  - `EFX_FF`: 10700
  - `EFX_RAM10`: 210
  - `EFX_DPRAM10`: 8
- 586 post-synthesis warnings existed; not yet fully triaged.

Earlier `efx_map.exe` runs on `D:\final_project` also passed after HDMI CDC changes:

```powershell
Set-Location D:\final_project\fpga\efinity
& 'D:\Efinity\2025.2\bin\efx_map.exe' --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_hdmi_cdc_v1 --output-dir work_syn_codex_hdmi_cdc_v1
```

Result recorded:

- exit code 0;
- no synthesis errors in `EFX.err.log`;
- warnings remain.

### Full flow caveat

An earlier full compile attempt reached map, then failed during debugger auto-instantiation:

```text
FileNotFoundError: No such file: work_syn_codex_fix_work_ascii\debug_top.v
```

Interpretation:

- Not a proven RTL syntax/map failure.
- It is likely tied to Efinity debug profile/debugger auto-instantiation setup.
- For bitstream closure, run the normal intended Efinity project flow from `D:\final_project`, or fix/disable the debug profile path.

## 7. Current Git / File State Snapshot

As of this handoff, `git status --short` showed modified RTL/XML files and untracked debug docs/module:

```text
 M final_project/fpga/efinity/mem_test.xml
 M final_project/fpga/rtl/dvi_tx/hdmi_top.v
 M final_project/fpga/rtl/framebuffer/ddr_buffer.v
 M final_project/fpga/rtl/framebuffer/ddr_rd_buffer.v
 M final_project/fpga/rtl/framebuffer/ddr_wr_buffer.v
 M final_project/fpga/rtl/framebuffer/elt_dcfifo_v10.v
 M final_project/fpga/rtl/framebuffer/fifo_d512t128.v
 M final_project/fpga/rtl/framebuffer/frame_buffer.v
 M final_project/fpga/rtl/framebuffer/frame_info_det.v
 M final_project/fpga/rtl/framebuffer/ser2par_24_128_v1.v
 M final_project/fpga/rtl/framebuffer/vid_info_det_v7.v
 M final_project/fpga/rtl/framebuffer/vid_par2ser.v
 M final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v
 M final_project/fpga/rtl/top/top.v
?? debug_records/
?? final_project/docs/architecture/dual_camera_hdmi_fix_plan_2026-07-06.md
?? final_project/docs/architecture/dual_camera_hdmi_pipeline_current_2026-07-06.md
?? final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v
```

Do not revert unrelated modifications. Treat them as active bring-up changes unless proven otherwise.

## 8. Faults Still To Diagnose

Prioritize these in order.

### P0. Confirm actual bitstream/build tree

Reason:

- User sees output that may match old dynamic fallback.
- D-drive/C-drive sync has caused confusion.

Checks:

- Confirm Efinity opened `D:\final_project\fpga\efinity\mem_test.xml`.
- Confirm `D:\final_project\fpga\rtl\dvi_tx\hdmi_top.v` contains `TEST_MODE(2'd2)`.
- Rebuild/flash from `D:\final_project`.
- If screen still cycles full-screen colors immediately, suspect old bitstream or another project copy.

Expected result after current code:

- If upstream path is not ready, HDMI should show stable static vertical color bars, not dynamic full-screen cycling.

### P1. Confirm reset/PLL/DDR release

Signals:

- `sys_pll_lock`
- `ddr_pll_lock`
- `pll_byteclk_locked`
- `arst_n`
- `rst_n`
- `ddr_cfg_ok`
- `sys_rst_n`
- `pixel_data_en`

Board landmark:

- `led[0] = ~ddr_cfg_ok`; off means DDR config complete.

If DDR is not configured, do not debug CSI/debayer/HDMI image quality yet.

### P2. Confirm CSI output format and activity

Signals per channel:

- `rx_out_vs`
- `rx_out_hs`
- `rx_out_de`
- `rx_out_data[39:0]`
- `rx_out_datatype`
- `rx_out_pixel_per_clk`

Expected:

- `datatype == 6'h2B`
- `pixel_per_clk == 4`
- VS/DE toggling in `i_sysclk_div2`

If not expected:

- If `datatype == 6'h2A` or `pixel_per_clk` indicates RAW8, the current RAW10 extraction is wrong for that observed mode, but do not blindly take low 32 bits. Either reconfigure camera/IP back to RAW10/4ppc or implement a real 40b-to-framebuffer repacker.
- If datatype/ppc is unstable, debug CSI lane/camera init/IP configuration first.

### P3. Confirm framebuffer write and read readiness

Signals:

- `frame_stable`
- `ddr_frame_len`
- `wr_frame_done`
- `rd_start`
- `ch0_frame_ready`, `ch1_frame_ready`
- `ch0_fifo_underflow`, `ch1_fifo_underflow`

Expected:

- after stable CSI frames, a DDR write completes;
- selected channel `frame_ready` eventually asserts;
- FIFO underflow stays low.

If `frame_ready` never asserts:

- inspect `frame_info_det` frame length and `ddr_frame_len`;
- verify write burst path and read start gating.

### P4. Confirm HDMI CDC bridge timing

Signals:

- `hdmi0_bridge_active`, `hdmi1_bridge_active`
- `hdmi0_bridge_underflow`, `hdmi1_bridge_underflow`
- selected bridge `vs/hs/de/data`

Expected:

- bridge active asserts after FIFO reaches start level;
- underflow remains low;
- output `de` active count becomes `1920 x 1080` in HDMI clock domain.

Open risk:

- The bridge writes every source clock, including blanking. This should preserve timing expansion, but if FIFO level drifts, inspect source/output total timing.
- Pixel order is currently even then odd. If live image appears but every pair is horizontally swapped, inspect this.

### P5. Confirm HDMI qualification

Signals inside/around `hdmi_top`:

- `hdmi_video_ready`
- `hdmi_input_stable`
- `input_h_act`
- `input_v_act`
- `input_h_active_error`
- `input_v_active_error`
- `input_v_total_error`
- `input_h_total_error`
- `input_h_sync_error`

Expected:

- input measured as `1920 x 1080`;
- four consecutive good frames;
- then `o_input_stable` asserts and HDMI switches from fallback to input.

If color bars remain:

- HDMI TX works, but `use_input_video` is false.
- Check which readiness/qualification term is blocking.

### P6. If live image appears but image is wrong

Then debug image quality in this order:

1. RAW10 truncation/exposure:
   - current design uses RAW10 bits `[9:2]`, discarding two LSBs.
   - if image is dark/low contrast, check sensor exposure/gain and whether truncation is acceptable.
2. Bayer pair order:
   - flip `CH0_BAYER_SWAP_PIXELS` and/or `CH1_BAYER_SWAP_PIXELS` one channel at a time.
3. Bayer phase inside `raw_to_rgb`:
   - confirm SC431HAI Bayer pattern against actual sensor output and line/column origin.
4. RGB ordering:
   - top-level debayer BGR-to-RGB repack may still need confirmation with a color target.
5. White balance:
   - currently bypassed. Re-enable only after base image/timing is correct.

## 9. Exhaustive Candidate Fault List

Use this as a checklist for systematic elimination.

### Build / flash / project copy

- wrong `mem_test.xml` opened in Efinity;
- flashing from old `D:\final_project` copy;
- C-drive edits not synced to D-drive;
- old bitstream remains on board;
- debug profile full flow fails after map and prevents generating new bitstream;
- Chinese path causes Efinity file read errors;
- `mem_test.xml` missing new `video_2pix_to_1pix_cdc.v`.

### Reset / clock / PLL

- `sys_pll_lock` or `ddr_pll_lock` not stable;
- `pll_byteclk_locked` not stable;
- `ddr_cfg_ok` not asserted;
- `pixel_data_en` never releases;
- decommissioned DSI lock accidentally still gates reset somewhere;
- clock constraints mismatch actual generated clock rates.

### Camera / CSI

- sensor not initialized;
- camera reset/power/I2C sequencing wrong;
- lane mapping/polarity wrong;
- CSI datatype not RAW10;
- `pixel_per_clk` not 4;
- CSI DE/VS unstable;
- byte-clock constraint/IP setting mismatch;
- one channel alive and the other dead, masked by channel selection.

### RAW packing / Bayer

- RAW10 extraction wrong only if observed format is not RAW10/4ppc;
- Bayer pair order wrong at `ch*_b/ch*_g` to debayer input;
- Bayer phase wrong in `raw_to_rgb`;
- even/odd pair order swapped by HDMI bridge;
- RGB/BGR byte order wrong after debayer.

### Framebuffer / DDR

- frame length detection wrong;
- write burst length/address issue;
- read starts before write completion;
- triple buffer bank selection stale;
- AXI arbitration between ch0/ch1 faulty;
- FIFO underflow or overflow;
- `frame_ready` gate too strict or too loose;
- frame output timing generated but data stale.

### HDMI bridge / timing

- `video_2pix_to_1pix_cdc` FIFO underflow;
- FIFO level never reaches `START_LEVEL`;
- source/HDMI clock ratio not exactly 1:2 in practice;
- DE expansion produces 960 instead of 1920 active pixels;
- blanking timing not expanded as expected;
- first output word phase wrong.

### HDMI top / display

- fallback hides upstream failure;
- `hdmi_top` active-size qualification rejects input;
- monitor/capture card expects a different HDMI timing;
- RGB/HS/VS/DE phase wrong into `dvi_encoder`;
- TMDS path is alive but input never selected.

## 10. Immediate Next Plan

### Step A: Lock the active build

1. In Efinity, open `D:\final_project\fpga\efinity\mem_test.xml`.
2. Confirm `D:\final_project\fpga\rtl\dvi_tx\hdmi_top.v` has `TEST_MODE(2'd2)`.
3. Confirm `D:\final_project\fpga\efinity\mem_test.xml` includes:

```text
../rtl/dvi_tx/video_2pix_to_1pix_cdc.v
```

4. Run map/PNR/bitstream from `D:\final_project`.
5. Flash the generated bitstream and record whether fallback is static bars or dynamic cycling.

### Step B: If fallback is still dynamic cycling

Stop RTL debugging and fix build/flash provenance:

- wrong project;
- old bitstream;
- D-tree not synced;
- debug/full-flow did not produce fresh bitstream.

### Step C: If fallback is static bars

HDMI physical output is correct, and upstream is not accepted. Probe readiness in this order:

```text
ddr_cfg_ok
pixel_data_en
rx_out_vs/de/datatype/pixel_per_clk
ch*_frame_ready
ch*_fifo_underflow
hdmi*_bridge_active
hdmi*_bridge_underflow
hdmi_video_ready
input_h_act/input_v_act/input_*_error
hdmi_input_stable
```

### Step D: If HDMI switches to live but image is noisy/striped

Then the pipe is at least timing-accepted. Debug:

```text
RAW10 extraction -> Bayer pair swap -> raw_to_rgb Bayer phase -> RGB repack -> DDR bank/order -> CDC pixel order
```

### Step E: Only after one live channel works

- Repeat on the second camera.
- Validate SW4 channel switching.
- Reconsider white balance and image-quality tuning.
- Clean warnings and run full Efinity signoff.

## 11. Files to Read First in New Window

Primary handoff and current architecture:

- `debug_records/camera_hdmi_handoff_2026-07-06.md`
- `debug_records/camera_hdmi_fix_log_2026-07-06.md`
- `debug_records/hdmi_dual_camera_bringup_summary_2026-07-05.md`
- `final_project/docs/architecture/dual_camera_hdmi_pipeline_current_2026-07-06.md`
- `final_project/docs/architecture/dual_camera_hdmi_fix_plan_2026-07-06.md`

Key RTL:

- `final_project/fpga/rtl/top/top.v`
- `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`
- `final_project/fpga/rtl/framebuffer/frame_buffer.v`
- `final_project/fpga/rtl/framebuffer/frame_info_det.v`
- `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`
- `final_project/fpga/rtl/dvi_tx/hdmi_top.v`
- `final_project/fpga/rtl/debayer/debayer_top_2to1.v`
- `final_project/fpga/rtl/debayer/raw_to_rgb.v`

Reference, not product baseline:

- `赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src\top.v`
- `赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src\dvi_tx\hdmi_top.v`
- `赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\ip_vendor\csi_rx_controller\settings.json`

## 12. Short Version for the Next Agent

Do not change RAW packing to byte-aligned RAW8 unless CSI status proves the camera is outputting RAW8. The current mainline should assume RAW10/4ppc: `PACK_BIT=40` is four RAW10 pixels, not five RAW8 pixels. The most urgent debug split is now:

1. Are we really flashing the current `D:\final_project` bitstream?
2. If yes, is HDMI fallback static bars?
3. If static bars, which readiness signal blocks `hdmi_input_stable`?
4. If live input is accepted but bad-looking, debug Bayer/pixel order rather than HDMI fallback.

