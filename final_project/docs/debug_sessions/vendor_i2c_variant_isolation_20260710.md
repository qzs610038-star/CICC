# Vendor I2C Variant Isolation Note

## Purpose

This note reserves a safe, reversible path for a possible SC431HAI I2C startup replacement. It exists because the current HDMI debug build repeatedly reaches the final stream-on write but does not reach `hdmi_top` input-video selection.

The replacement must be created as a separate Efinity project and separate RTL files. It must not overwrite the current debug tree, vendor demo files, constraints, generated IP, or `outflow` artifacts.

## Current Status

The current worktree now uses the clean replacement controller in
`fpga\rtl\mipi_csi\cam_i2c_ctrl\i2c\i2c_master_ctrl_top.v`.

The pre-rewrite worktree was frozen first in:

```text
final_project\archives\fpga_before_vendor_i2c_rewrite_20260710.zip
SHA-256: 8B7BBF8D612F25FC33D140153003A0B885D81F34E752F624FEF8C4EEE81DD9A1
```

The archive was extracted and verified after creation. It contains the pre-rewrite RTL tree, current Efinity project and constraint files, and the relevant debug notes. It is the recovery source for the previous state.

The active manual-build tree remains:

```text
C:\Users\20306\Desktop\赛题资料\CICC\final_project
D:\final_project
```

The active project remains `fpga\efinity\mem_test.xml`. Its RTL is currently a dirty debug worktree; it is not a pristine original snapshot. Do not label it as a known-good I2C baseline.

## Source Boundaries

| Role | Location | Write rule |
|---|---|---|
| Current debug implementation | `final_project\fpga\rtl\` | Preserve; do not overwrite for the variant. |
| Current manual-build mirror | `D:\final_project\fpga\rtl\` | Preserve; sync only deliberate current-debug changes. |
| Official SC431HAI reference | `赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src\mipi_csi\` | Read-only reference. |
| Future isolated variant | `final_project\fpga\variants\vendor_i2c_baseline\` | New files only. |
| Future isolated Efinity project | `final_project\fpga\efinity\mem_test_vendor_i2c.xml` | New XML only. |

The official reference controller uses `DATA_LENGTH=161` and has its last configuration entry at `ROM[8'ha0]`. The current debug controller uses `DATA_LENGTH=162` and adds a final `ROM[8'ha1] = {16'h0100, 8'h01, 1'b0}` stream-on write. This difference is the reason to test a separate baseline, not a reason to edit the vendor files in place.

## Required Variant Layout

The originally reserved separate-project layout remains valid for a future parallel test. The current time-critical test instead replaces only `i2c_master_ctrl_top.v` after the verified archive was created; it does not overwrite the vendor demo files, project XML, constraints or generated outputs.

For a future parallel project, create only these new files:

```text
final_project\fpga\variants\vendor_i2c_baseline\
  README.md
  i2c_16addr_8data_vendor.v
  i2c_master_ctrl_top_vendor.v
  i2c_master_reg_set_vendor.v
  i2c_master_reg_rom_vendor.v
  soft_mipi_rx_top_vendor_i2c.v
  top_vendor_i2c.v

final_project\fpga\efinity\mem_test_vendor_i2c.xml
```

The variant may reuse existing generic `i2c_master` support modules only if their module names do not collide. Variant-specific module names must end in `_vendor` or `_vendor_i2c`.

`top_vendor_i2c.v` must retain the current video, framebuffer, CSI and HDMI implementation. Only the two `soft_mipi_rx_top` instantiations may change to `soft_mipi_rx_top_vendor_i2c`.

`mem_test_vendor_i2c.xml` must be copied from `mem_test.xml`, then changed only to use `top_vendor_i2c.v` and the new variant files. Keep `constrain.sdc`, `mem_test.peri.xml`, IP configuration and debugger profile unchanged unless an independent review identifies a required change.

## Recovery Procedure

Before creating a variant, record the current state without changing it:

```powershell
Set-Location 'C:\Users\20306\Desktop\赛题资料\CICC'
git status --short
git diff -- final_project/fpga/rtl
git diff --check
```

Compare a current file with the official reference without copying either file:

```powershell
Compare-Object `
  (Get-Content -Encoding utf8 '赛方提供材料\TJ375N529_SC431HAI2LCD_Demo_V3\src\mipi_csi\cam_i2c_ctrl\i2c\i2c_master_reg_rom.v') `
  (Get-Content -Encoding utf8 'final_project\fpga\rtl\mipi_csi\cam_i2c_ctrl\i2c\i2c_master_reg_rom.v')
```

To return to the frozen pre-rewrite implementation, extract `fpga_before_vendor_i2c_rewrite_20260710.zip` to a temporary directory and copy back only the required files after comparing hashes. Do not use `git reset --hard` or overwrite files from the vendor demo as a recovery method.

## Acceptance Gate

The variant is only a useful replacement if all conditions hold on the board:

1. Efinity completes synthesis, place-and-route and timing analysis without new blocking errors.
2. The selected camera reaches I2C configuration and stream-on without a persistent NACK indication.
3. MIPI byte clock, CSI data, framebuffer write/read and `hdmi_top` input selection are observed before evaluating image quality.
4. The HDMI image changes from fallback bars to a camera-derived image.

If conditions 1-3 pass but the picture is still noisy, keep the vendor-I2C variant and resume debugging from RAW10/CSI/framebuffer/HDMI ordering. Do not fold that downstream fault into I2C conclusions.
