# HDMI noise bands debug record

- Date: 2026-07-05
- Scene: HDMI output after fallback bypass.
- Symptom: The screen shows flashing orange/cyan horizontal noisy bands instead of clean color bars or live camera video.
- Interpretation: HDMI is now receiving selected camera-path timing/data, but the upstream frame data is not yet reliable enough for display.

## Fixes applied

- Restored the HDMI input-stability gate so unstable camera timing falls back to the internal test pattern instead of showing invalid frame data.
- Fixed `frame_info_det.v` frame stability detection to compare actual frame pixel counts, not a one-bit frame-end pulse.
- Required three matching non-trivial frame lengths before marking a framebuffer input stable.
- Reordered debayer output from BGR to RGB before HDMI/AWB use.
- Bypassed white balance for HDMI bring-up with `HDMI_BYPASS_WHITE_BALANCE = 1'b1`.
- Fixed `white_balance.v` `vs_d` update and guarded average divisions against zero.

## LED naming note

- `led[2]` is the RTL bus bit used for channel selection.
- The board LED observed by the user for that same signal is `LED20`.
