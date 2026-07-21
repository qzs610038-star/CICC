# PC HDMI Overlay Candidate

This is a PC-only visual continuity fallback. It draws a foreground box and
heuristic color/shape/size text over pixels from a synthetic source, a video
file, or a Windows UVC camera. A USB HDMI capture card appears as a UVC camera.

It does **not** prove FPGA recognition, CPU Hello, UART, APB, OSD, HDMI board
output, or any board-level result. It contains no serial, MMIO, `pymycobot`, or
robot-control path.

## Current status (2026-07-21)

- `HOST_SYNTHETIC`: available for offline development and tests.
- Local camera: `Honor Camera` was present during preparation but is not an
  HDMI source and was not opened automatically.
- `HDMI_UVC`: `BLOCKED_NO_CAPTURE_DEVICE`; the board is held by libaoxun and no
  HDMI capture device is currently enumerated.
- `ARM_ENABLED=0`, `BOARD_VERIFIED=NO`.

## Offline run without a board

```powershell
cd D:\CICC-pc-hdmi-demo-20260721\competition_project_single_camera\tools\pc_hdmi_demo
.\run_pc_hdmi_demo.ps1 -Mode synthetic
```

Headless evidence run:

```powershell
$run = Join-Path $env:TEMP 'cicc-pc-hdmi-synthetic'
.\run_pc_hdmi_demo.ps1 -Mode synthetic -Headless -Frames 180 -OutputDir $run
Get-Content -Raw (Join-Path $run 'run_summary.json')
```

## When a USB HDMI capture card and board become available

1. Connect `board HDMI OUT -> capture card HDMI IN -> PC USB 3.x`.
2. Enumerate readable video indexes without starting the demo:

   ```powershell
   .\run_pc_hdmi_demo.ps1 -ListDevices
   ```

3. Start with an empty scene so the first frame is a usable background. Replace
   the device index with the capture card index:

   ```powershell
   .\run_pc_hdmi_demo.ps1 -Mode camera -Device 1 -SourceLabel HDMI-UVC
   ```

4. Press `B` with an empty scene to refresh the background after exposure or
   camera-position changes.

Keys: `B` capture empty background, `R` reset, `S` save a frame when
`-OutputDir` is set, `Q`/`Esc` quit.

The first real HDMI gate must record the UVC device identity, requested and
actual resolution/FPS, a continuous frame run, a device-unplug failure, and a
screenshot containing `SOURCE=HDMI-UVC`. Until then, keep
`HDMI_UVC_VERIFIED=NO`.

## Tests

```powershell
python -m unittest discover -s .\tests -v
```
