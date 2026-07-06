# Camera HDMI RAW path fix log

- Date/time: 2026-07-06 16:53:15 +08:00
- Scenario: Dual MIPI CSI camera path through DDR/framebuffer/debayer to HDMI.
- Target tree: repo-relative `final_project`
- Build path used for Efinity: `D:\cicc_cbm_link\final_project\fpga\efinity`
- Tool: Efinity 2025.2.288.4.15, compiled Mar 30 2026.

## Symptom / Issue List

1. External diagnosis said `top.v` extracted RAW8 bytes on 10-bit spacing and mixed pixels.
   - Result after source check: the diagnosis correctly noticed a width/format boundary, but the proposed RAW8 byte-aligned fix is not correct for this project.
   - Evidence: vendor CSI controller exposes `datatype`, `pixel_per_clk`, and 64-bit `pixel_data`; project/demos use RAW10 with `PACK_BIT = 40`.
   - Correct interpretation: `PACK_BIT = 40` is 4 RAW10 pixels per clock, not 5 RAW8 pixels per clock.

2. External diagnosis said `PACK_BIT=40` vs `I_VID_WIDTH=32` drops one 8-bit pixel per clock.
   - Result after source check: not accepted as stated.
   - Evidence: a 40-bit word is 4 RAW10 pixels. Converting it to framebuffer `I_VID_WIDTH=32` should keep the MSB 8 bits of each RAW10 pixel.
   - Fix direction: keep `PACK_BIT=40`; make the RAW10-to-RAW8 truncation explicit.

3. External diagnosis said `{ch0_g,ch0_b}` to `{ch0_b,ch0_g}` might swap Bayer phase.
   - Result: accepted as a real bring-up risk.
   - Evidence: `frame_buffer.vout` is a 16-bit Bayer two-pixel stream feeding `debayer_top_2to1.raw_datax4_i`, not an RGB triplet.
   - Fix direction: add per-channel localparams so the two Bayer bytes can be flipped without rewiring multiple module instances.

4. External diagnosis said `frame_buffer.vout` has no independent R component.
   - Result after source check: not a bug by itself.
   - Evidence: this stage is still Bayer RAW. The R/G/B components are generated later by `debayer_top_2to1`.

5. HDMI RGB format concern.
   - Result: current path is acceptable for this fix scope.
   - Evidence: upstream debayer output is converted to RGB byte order before `hdmi_top`; `hdmi_top` feeds `dvi_encoder` as `red_din/green_din/blue_din`.
   - Remaining risk: visual color order still needs board confirmation with a known color target.

6. `video_2pix_to_1pix_cdc` first-pixel timing concern.
   - Result: accepted.
   - Evidence: local `DC_FIFO` is synchronous read; data is valid one read clock after `RdEn`.
   - Fix direction: request a FIFO word first, then assert `o_de/o_data` only after the read word is available.

## Code Changes

### `final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v`

- Added diagnostic outputs:
  - `rx_out_datatype[5:0]`
  - `rx_out_pixel_per_clk[3:0]`
- Connected CSI controller `pixel_data` to internal `csi_pixel_data[63:0]`, then exported `rx_out_data = csi_pixel_data[PACK_BIT-1:0]`.
- Exported CSI controller `datatype` and `pixel_per_clk`.
- Kept `PACK_BIT=40` behavior intact.
- Changed local `vid_info_det` reset to `reset_pixel_n`, so the detector follows the pixel-domain reset rather than data-enable.

### `final_project/fpga/rtl/top/top.v`

- Added `raw10_4pix_to_raw8_4pix()`:
  - RAW10 pixel 3: `[39:32]`
  - RAW10 pixel 2: `[29:22]`
  - RAW10 pixel 1: `[19:12]`
  - RAW10 pixel 0: `[9:2]`
- Replaced inline `vin` extraction with:
  - `ch0_raw8_4pix = raw10_4pix_to_raw8_4pix(rx_out_data[39:0])`
  - `ch1_raw8_4pix = raw10_4pix_to_raw8_4pix(rx_out_data1[39:0])`
- Added CSI format checks:
  - `datatype == 6'h2B`
  - `pixel_per_clk == 4`
- Added Bayer byte-order switches:
  - `CH0_BAYER_SWAP_PIXELS`
  - `CH1_BAYER_SWAP_PIXELS`
- Default switch value is `1'b1`, preserving the current `{ch*_b,ch*_g}` order while making Bayer phase easy to flip during board debug.
- Routed selected CSI-format OK through a two-flop HDMI-clock synchronizer and included it in `led[3]`.
  - `led[3] = hdmi_input_stable & selected_csi_format_ok`
  - This is diagnostic only; it does not gate the HDMI video data path.
- Instantiated `video_2pix_to_1pix_cdc` for both HDMI candidate channels.

### `final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v`

- Reworked the read-side state machine for synchronous FIFO read latency.
- `PH_REQUEST` asserts FIFO read and blanks output.
- `PH_LOW` consumes FIFO output only when the previous read is valid.
- `PH_HIGH` emits the upper 24-bit pixel from the stored 48-bit word.
- `o_de/o_data` no longer advance before the first FIFO word is valid.
- Added `o_active` and `o_underflow` behavior for top-level HDMI readiness diagnostics.

### `final_project/fpga/efinity/mem_test.xml`

- Confirmed `../rtl/dvi_tx/video_2pix_to_1pix_cdc.v` is included as a design file.
- No XML reshuffling was intentionally performed in this fix pass.

## Verification

### Whitespace / diff check

Command:

```powershell
git diff --check -- final_project/fpga/rtl/mipi_csi/soft_mipi_rx_top.v final_project/fpga/rtl/top/top.v final_project/fpga/rtl/dvi_tx/video_2pix_to_1pix_cdc.v
```

Result:

- No whitespace errors were reported.
- Git warned that some files may be normalized from CRLF to LF when Git next touches them.

### Efinity map-only

Command:

```powershell
Set-Location D:\cicc_cbm_link\final_project\fpga\efinity
cmd /c "call D:\Efinity\2025.2\bin\setup.bat && D:\Efinity\2025.2\bin\efx_run.bat mem_test.xml --prj -f map --output_dir work_syn_codex_fix_map_final_ascii --work_dir work_syn_codex_fix_map_final_work_ascii"
```

Result:

- `map : PASS`
- Log line: `Stage completed: map`
- Resource summary:
  - `EFX_LUT4`: 12210
  - `EFX_FF`: 10700
  - `EFX_RAM10`: 210
  - `EFX_DPRAM10`: 8
- Log reported 586 post-synthesis netlist warnings; these are not new blockers from the RAW extraction fix, but must not be ignored for final timing/bitstream signoff.

### Full compile attempt boundary

An earlier full compile attempt in the ASCII path reached map successfully, then failed during debugger auto-instantiation:

```text
FileNotFoundError: No such file: work_syn_codex_fix_work_ascii\debug_top.v
```

Interpretation:

- This is not an RTL map failure.
- It comes from the Efinity debug flow configured by the current project/debug profile.
- For bitstream closure, either fix the debugger auto-instantiation setup or run the normal project flow with the intended debug settings.

### Path issue

Running Efinity directly from the Chinese workspace path failed with:

```text
filesystem error: Cannot convert character sequence: Illegal byte sequence
```

Workaround used:

- Build through `D:\cicc_cbm_link`, an ASCII junction to the same repo.

## Remaining Risks / Next Actions

1. Board-level CSI format confirmation:
   - Observe `rx_out_datatype == 6'h2B`.
   - Observe `rx_out_pixel_per_clk == 4`.
   - `led[3]` now requires selected CSI RAW10/4ppc plus stable HDMI input timing.

2. Bayer phase confirmation:
   - If colors or mosaic phase are wrong, flip `CH0_BAYER_SWAP_PIXELS` and/or `CH1_BAYER_SWAP_PIXELS`.
   - Use a color target with red/green/blue regions; do not judge only from a low-texture scene.

3. HDMI/CDC rate confirmation:
   - `video_2pix_to_1pix_cdc` assumes the 2-pixel source stream and 1-pixel HDMI stream remain rate-compatible.
   - Watch `selected_bridge_underflow` and HDMI fallback behavior during long runs.

4. Full Efinity flow:
   - Map-only passes.
   - PNR/bitstream not completed in this pass because full compile hit debug auto-instantiation setup, not RTL syntax.

5. Warning cleanup:
   - Current map log has many existing unconnected/truncated-width warnings.
   - Before final hardware signoff, triage warnings touching AXI width, framebuffer burst length, and top-level unconnected ports.
