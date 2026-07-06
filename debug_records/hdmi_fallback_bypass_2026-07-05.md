# HDMI fallback bypass debug record

- Date: 2026-07-05
- Scene: HDMI output after S0 default channel and SW4 channel-toggle changes.
- Symptom: The screen still showed a full-screen changing color, sometimes with white/yellow/blue vertical bars. This matches the internal `color_bar_rgb(TEST_MODE=2'd1)` fallback in `hdmi_top.v`, not live camera video.
- Impact: HDMI could remain in the fallback pattern whenever `vid_info_det.frame_stable` stayed low, so the observed color bars did not prove whether S0 camera data had reached HDMI.

## Files changed

- `final_project/fpga/rtl/dvi_tx/hdmi_top.v`
- `final_project/fpga/rtl/top/top.v`

## Fix

- Added `USE_INPUT_STABLE_GATE` to `hdmi_top.v`, defaulting to `1'b0` for bring-up.
- Changed HDMI mux select to `use_input_video = video_path_ready & (!USE_INPUT_STABLE_GATE || i_stable)`.
- Kept `o_input_stable = video_path_ready & i_stable` for diagnostics.
- Kept S0 as the reset/default channel and preserved SW4 channel toggling.
- Changed RTL `led[3]` to `hdmi_video_ready`, so the board indicates when HDMI is allowed to use the camera path.
- Drove `i_cfg_vid` with `24'd0` and connected `v_active_error()` explicitly to remove local HDMI warnings.

## Sync

- Copied the updated `top.v` and `hdmi_top.v` from the C drive workspace to `D:\final_project`.
- SHA256 hashes matched after copy for both files.

## Verification

Command run from `D:\final_project\fpga\efinity`:

```powershell
& 'D:\Efinity\2025.2\bin\efx_map.exe' --project-xml mem_test.xml --root top --family Titanium --device TJ375N529 --work-dir work_syn_codex_hdmi_bypass --output-dir work_syn_codex_hdmi_bypass
```

- Exit code: 0
- New log directory: `D:\final_project\fpga\efinity\work_syn_codex_hdmi_bypass`
- `EFX.err.log`: no errors
- `EFX.warn.log`: no `hdmi_top`, `USE_INPUT_STABLE_GATE`, `i_cfg_vid`, or `v_active_error` warnings
- Existing project warnings remain: implicit nets in older modules, unconnected top-level ports, and white_balance DSP CE warnings.

## Next board-test interpretation

- RTL `led[2]` off means S0 selected. RTL `led[2]` on means S1 selected.
- User board observation: RTL `led[2]` is the physical LED20 indicator.
- RTL `led[3]` on means HDMI is allowed to use camera input instead of the fallback color bar.
- If RTL `led[3]` is on and the cycling color bars disappear, HDMI fallback has been bypassed; continue debugging CSI/DDR/framebuffer/debayer/white_balance.
- If RTL `led[3]` is on but the screen is black or no-signal, HDMI is using the camera path but the selected S0 pipeline is not producing valid timing/data.
- If RTL `led[3]` is on and the exact same cycling color bars remain, the wrong bitstream or wrong project copy was likely programmed.
