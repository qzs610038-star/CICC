# A11 Synthetic Preprocess Isolated Map Record

> Date: 2026-07-12
> Goal: Verify that the synthetic five-color source can enter the FPGA ROI, color-area, bbox, center, and snapshot logic without changing the verified `D:\final_project` HDMI baseline.
> Status: Isolated-project map passed. PNR, bitstream, download, GUI Debugger capture, and board-level snapshot values are NOT VERIFIED.

## Scope

- Active D-drive build/flash baseline `D:\final_project\fpga` was not modified.
- Isolated project: `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga`.
- The copy starts from the D-drive five-color HDMI baseline. The source `.lock` was held by Efinity and was not copied; it is not HDL or project input.
- No changes to `constrain.sdc`, `mem_test.peri.xml`, PLL, JTAG, SoC, CPU/APB, OSD, or robot files.
- Copied historical generated folders are stored below `efinity\baseline_generated_archive`; this run writes only `outflow_a11_v2` and `work_syn_a11_v2`.

## Minimal Delta

The isolated project adds six preprocessing RTL files plus the C-drive ch1 tap in `top.v`:

- `vision_stream_adapter_2ppc.v`
- `roi_window_2ppc.v`
- `pixel_mask_2ppc.v`
- `feature_accumulator_2ppc.v`
- `feature_snapshot.v`
- `vision_preprocess_channel.v`

`PREPROCESS_CH1_USE_SYNTHETIC_SOURCE` and `HDMI_USE_SYNTHETIC_VERIFY` are both enabled only in the isolated candidate. The synthetic source feeds the new preprocessing tap and the dedicated HDMI CDC. Live CSI/DDR/HDMI instances remain present; no CPU, APB, OSD, or external output is driven by the tap.

## Byte Order Correction

The HDMI-correct synthetic source uses `{R1,G1,B1,R0,G0,B0}`. The existing preprocessing adapter expects the live Debayer contract `{B1,G1,R1,B0,G0,R0}`. Without conversion, yellow would be interpreted as cyan and red/blue would be exchanged by the preprocess path.

`preprocess_ch1_synthetic_bgr_2ppc` now converts only the preprocessing branch. The HDMI branch keeps the original RGB source, so this correction does not alter the verified HDMI yellow display.

## Map Verification

Command:

```powershell
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && cd /d C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity && efx_run.bat mem_test --prj -f map --work_dir work_syn_a11_v2 --output_dir outflow_a11_v2 --timeout 600"
```

Result: `map : PASS`, exit code 0.

- The map report analyzes, compiles, and hierarchically pre-synthesizes `vision_preprocess_channel`, `feature_accumulator_2ppc`, and `feature_snapshot`.
- No `ERROR` or `FATAL` was reported.
- The full project retains 1098 post-synthesis warnings. They are not approved as ignorable.
- Final v2 resource summary: `EFX_ADD=1827`, `EFX_LUT4=10339`, `EFX_FF=7991`, `EFX_RAM10=154`.
- Debug profile automatically finds 11 `i_sysclk_div2` probes: snapshot valid, frame ID, ROI pixels, red/blue/yellow area, foreground area, bbox min/max, center, and status.

## GUI Capture Criteria

Use Efinity GUI Debug Wizard on the A11 project to generate an A11-specific `.dbg.vdb` from `outflow_a11_v2\mem_test.elab.vdb`. Select `JTAG USER TAP = USER2`: `JTAG_USER1` is already declared by `jtag_inst1` in `mem_test.peri.xml`, while the top level reserves `jtag_inst2_*` ports for the second user JTAG resource. Only then may A11 proceed to PNR and candidate download. Do not copy a VDB or bitstream from D drive.

For the 960 x 1080 source, the square is 320 x 320 at `x=320..639`, `y=380..699`:

| Color | Expected color area | Expected fg area | bbox min/max | Center |
|---|---:|---:|---|---|
| Red | `red_area=102400` | `102400` | `{380,320}` / `{699,639}` | `{539,479}` |
| Blue | `blue_area=102400` | `102400` | same | same |
| Yellow | `yellow_area=102400` | `102400` | same | same |
| White | color areas 0 | `102400` | same | same |
| Black | color areas 0 | `102400` | same | same |

The packed bbox and center format is `{y,x}`. `snapshot_valid` may be one `i_sysclk_div2` cycle because ACK is tied high; use it or increasing `frame_id` as the capture trigger.

## Evidence

- `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a11_v2\mem_test.map.out`
- `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a11_v2\mem_test.res.csv`
- `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow_a11_v2\debug_profile.mark_debug.json`
- Current D-drive HDMI bitstream SHA-256: `E1C06B073A729D6DFADC309EEAE250649B91A5A27F097D1FDE25F9E8C3436E3C`.

## Debugger PNR And Bitstream Result

- First auto-instantiation attempt at 20:13 used `USER1` and failed because the existing peripheral block already occupies `JTAG_USER1`.
- The corrected Debug Wizard profile uses `USER2` and preserves the same 11 `preprocess_ch1_*` probes.
- GUI PNR completed successfully at 20:19 and produced:
  - `outflow\mem_test.dbg.vdb`
  - `outflow\mem_test.bit`
  - `outflow\mem_test.hex`
- Timing: minimum setup slack `1.577 ns` on `axi0_ACLK`; minimum hold slack `0.026 ns` on `i_sysclk_div2` / MIPI RX clocks.
- CDC report: no synchronizer warnings.
- Existing/project warnings remain in `outflow\EFX.warn.log`; they were not classified as harmless.
- No download or board observation was performed by this run.

## Next Manual Board Step

Load only `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\outflow\mem_test.bit` to FPGA SRAM using the existing JTAG download flow. This is not a persistent flash operation. Then open the Efinity Debugger with the paired A11 `mem_test.dbg.vdb` and capture the 11 probes while the five-color square cycles.

Do not use `D:\final_project\fpga\efinity\outflow\mem_test.bit` for this test. After A11 board capture, restore the D-drive baseline by downloading its own bitstream if needed.

## First Board Capture: Yellow Frame

At 2026-07-12 20:27, the A11 bitstream was loaded through JTAG SRAM and the USER2 logic-analyzer core completed a `Run Immediate` capture. The GUI returned from `Full` to `Idle` without rendering a visible waveform panel, but it wrote `efinity\la0_waveform.vcd`; the VCD contains valid sampled values from sample 0 to sample 2047.

Decoded stable sample values:

| Probe | Captured value | Expected | Result |
|---|---:|---:|---|
| `preprocess_ch1_yellow_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_red_area` | `0` | `0` | PASS |
| `preprocess_ch1_blue_area` | `0` | `0` | PASS |
| `preprocess_ch1_fg_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_roi_pixel_count` | `1036800` | `960 * 1080 = 1036800` | PASS |
| `preprocess_ch1_bbox_min` | `{380,320}` | `{380,320}` | PASS |
| `preprocess_ch1_bbox_max` | `{699,639}` | `{699,639}` | PASS |
| `preprocess_ch1_center` | `{539,479}` | `{539,479}` | PASS |
| `preprocess_ch1_status` | `0` | no ROI/bbox fault | PASS |

`snapshot_valid` was sampled low because it is only one `i_sysclk_div2` cycle when `i_snapshot_ack` is tied high. The changing `frame_id` confirms the capture occurred during active frame processing. This proves the board-level synthetic yellow pixel stream reaches the preprocess tap with correct RGB/BGR adaptation and correct area/geometry statistics.

The capture file is `C:\fpga_soc_isolated\tj375_synthetic_preprocess_a11\fpga\efinity\la0_waveform.vcd`.

## Second Board Capture: Red Frame

At 20:30, a second `Run Immediate` capture overwrote the same VCD and captured a red frame:

| Probe | Captured value | Expected | Result |
|---|---:|---:|---|
| `preprocess_ch1_red_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_blue_area` | `0` | `0` | PASS |
| `preprocess_ch1_yellow_area` | `0` | `0` | PASS |
| `preprocess_ch1_fg_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_roi_pixel_count` | `1036800` | `1036800` | PASS |
| geometry/status | unchanged and valid | same as yellow row | PASS |

The frame ID changed from the yellow capture, confirming a new frame was captured. The VCD path is reused and therefore contains only the latest red capture; the yellow values above remain recorded evidence from the first capture.

## Third Board Capture: Blue Frame

At 20:33, a third `Run Immediate` capture overwrote the VCD and captured a blue frame:

| Probe | Captured value | Expected | Result |
|---|---:|---:|---|
| `preprocess_ch1_blue_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_red_area` | `0` | `0` | PASS |
| `preprocess_ch1_yellow_area` | `0` | `0` | PASS |
| `preprocess_ch1_fg_area` | `102400` | `102400` | PASS |
| `preprocess_ch1_roi_pixel_count` | `1036800` | `1036800` | PASS |
| geometry/status | unchanged and valid | same as yellow row | PASS |

Red, blue, and yellow color-area counters have now each been verified on board using the isolated A11 bitstream.

## Fourth Board Capture: White Frame

At 20:36, the operator visually confirmed that HDMI showed the white square when `Run Immediate` was pressed. The captured feature values were:

| Probe group | Captured value | Expected | Result |
|---|---:|---:|---|
| red/blue/yellow area | all `0` | all `0` | PASS |
| `fg_area` | `102400` | `102400` | PASS |
| `roi_pixel_count` | `1036800` | `1036800` | PASS |
| bbox/center/status | valid geometry / `0` status | same | PASS |

The current 11-probe profile does not include `sum_r/sum_g/sum_b` or a white/black classifier result, so the white identity comes from the concurrent HDMI observation. It proves that white is foreground despite having no red/blue/yellow area. It does not prove FPGA white-versus-black classification.

## Fifth Board Capture: Black Frame

At 20:37, the operator visually confirmed that HDMI showed the black square when `Run Immediate` was pressed. The overwritten VCD (`la0_waveform.vcd`, last write time `2026-07-12 20:37:45`) contains:

| Probe group | Captured value | Expected | Result |
|---|---:|---:|---|
| red/blue/yellow area | all `0` | all `0` | PASS |
| `fg_area` | `102400` | `102400` | PASS |
| `roi_pixel_count` | `1036800` | `1036800` | PASS |
| bbox | `{y=380, x=320}..{y=699, x=639}` | same | PASS |
| center/status | `{y=539, x=479}` / `0` | same | PASS |

The current probe profile cannot distinguish black from white by itself. Therefore the black identity is established by the concurrent HDMI observation, while the VCD independently verifies that the expected foreground square and geometry reached the FPGA preprocessing snapshot.

## NOT VERIFIED

- Red, blue, yellow, white, and black synthetic rows are board-verified. White and black identities are tied to concurrent HDMI observation because the current probe profile cannot distinguish them independently.
- Icarus simulation: `iverilog.exe` is not installed on this host.
- Real CSI/Debayer input, CPU/APB/OSD, size calibration, and robot behavior.

## Superseded By A12

The original A11 GUI Debugger configuration, PNR, timing, bitstream, JTAG SRAM download, HDMI output, and 11-probe samples are board-verified. Their evidence is the generated A11 files and captures recorded above. A12 replaces only the white/black observability limitation: it adds existing RGB/luma frame sums to a new 15-probe Debug Wizard run and must generate its own `.dbg.vdb` before PNR or download.

## Debugger Attempt Log

- 2026-07-12 20:13: Auto-instantiation was first attempted with `USER1` and failed with `Jtag resource = JTAG_USER1 has been occupied`.
- Cause: `JTAG_USER1` is already declared by the project peripheral block `jtag_inst1`; the failure is a resource conflict, not a preprocessing RTL or PNR failure.
- Correction: rerun Debug Wizard with the same 11 probes and select `USER2`. No D-drive file, RTL, constraint, or `.peri.xml` change is required for this correction.
