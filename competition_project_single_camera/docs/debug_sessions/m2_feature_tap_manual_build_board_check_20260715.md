# M2 Feature Tap Manual Build and Board Check

## Scope

This check is only for the disabled ch0 RGB feature-tap gate. It proves that
the new read-only RTL can coexist with the verified camera pipeline. It does
not prove CPU, SoC, APB, UART, OSD, feature classification, or arm control.

## D-Drive Sources

The following files are synchronized from
`competition_project_single_camera/` to `D:\TJ375N529_SC431HAI2LCD_Demo_V3`:

| File | SHA-256 |
|---|---|
| `src/top.v` | `746BAD51743E46DBCD2D89DBFF8CED3A89F00999DF0B5312B62A2D00B1C5D343` |
| `src/feature_stats/feature_stats_tap.v` | `F2E132C6BBA1934AAF7B0E2D8E306990890E21ADBC7B03F45BF2A05F194FE6FC` |
| `mem_test.xml` | `F0D01C43D483176F7754952606CF8502F1850766ACB12EFE14CFB8E9BD26F3ED` |

## Intended Runtime Behavior

`src/top.v` instantiates `u_ch0_feature_stats_tap` from the Debayer output but
ties `i_capture_enable` to `1'b0`. The module has no output path to HDMI, DDR,
LEDs, CSI, or the camera I2C configuration. This build must therefore retain
the current ch0 HDMI behavior; feature capture is intentionally unavailable.

## Operator Steps

1. Open `D:\TJ375N529_SC431HAI2LCD_Demo_V3\mem_test.xml` in Efinity 2025.2.288.4.15.
2. Run the complete flow: Map, PNR, bitstream generation. Do not reuse the old
   `outflow/mem_test.bit`.
3. Record map resource usage and all new warnings.
4. Record minimum Setup and Hold slack from `outflow/mem_test.timing.rpt`.
5. Record the CDC conclusion from the new report; do not treat warnings as
   ignorable without review.
6. Record SHA-256 and timestamp of the new `outflow/mem_test.bit`.
7. Burn the new bitstream, use J48/ch0, and verify the normal HDMI camera view.
8. Check LED20/F3 is stable and LED21/F2 remains off under the existing
   interpretation. Do not use SW4 diagnostic as a classification test.

## Pass Criteria

- Map, PNR, and bitstream finish without errors.
- Setup and Hold slack are both non-negative.
- CDC has no new unresolved warning.
- The new bitstream is newer than the 2026-07-15 14:06:11 baseline.
- J48/ch0 HDMI camera image has no regression during a 10-minute observation.

## Explicit Non-Goals

- No feature snapshot is visible or readable: capture is disabled.
- No QCRV32, SoC, APB, UART, GPIO, OSD, or arm function is included.
- No change is made to `constrain.sdc`, `.peri.xml`, IP settings, framebuffer,
  Debayer, HDMI, or camera sensor registers.

## Report Back

Return the Map resource summary, minimum Setup/Hold lines, CDC summary,
bitstream SHA-256/timestamp, and one normal J48/ch0 HDMI screenshot. If any
item fails, stop here and provide the original report/screenshot before
attempting another source change.

## Returned Evidence (2026-07-15)

| Item | Result |
|---|---|
| Map / PNR / bitstream | PASS; new output chain completed on the D-drive mirror. |
| Map resources | `EFX_LUT4=10887`, `EFX_FF=9434`, `EFX_RAM10=163`, `EFX_DPRAM10=4`. |
| Setup minimum slack | `+1.766 ns` (`axi0_ACLK -> axi0_ACLK`). |
| Hold minimum slack | `+0.026 ns` (the minimum of the reported clock-domain pairs). |
| CDC | PASS; `No Synchronizer warnings to report.` |
| Bitstream | `outflow/mem_test.bit`, 2026-07-15 15:32:07, SHA-256 `45427C12AFE874C6032614B3D241EFD3BCBABFF395970D4D80FFFE8165F78535`. |
| Board HDMI | User confirmed normal J48/ch0 camera output with no observed regression; LED20/F3 on and LED21/F2 off were previously reported for this path. |

The D-drive source hashes after the successful build are
`src/top.v=746BAD51743E46DBCD2D89DBFF8CED3A89F00999DF0B5312B62A2D00B1C5D343`
and
`src/feature_stats/feature_stats_tap.v=10A5D6D6DFE1148ECC4561F24C4F5E41436E43FBD105130A21885A35B072A465`.

`i_capture_enable` remains tied to `1'b0`. This bitstream contains no active
feature snapshot, SoC, CPU, UART, APB, OSD result, GPIO business input, or arm
control function. A 10-minute continuous observation was not recorded in this
check and remains NOT VERIFIED.
