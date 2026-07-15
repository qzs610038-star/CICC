# M2 Hard SoC / Video Resource Conflict Report

> Date: 2026-07-15
>
> Decision required before any SoC insertion into the single-camera video
> project.

## Confirmed Resource Table

| Resource | Current video owner | Function | Board evidence |
|---|---|---|---|
| `PLL_TR0` | `MIPI_TX_PLL` | MIPI TX clock from 25 MHz | Interface Designer screenshot `142629` |
| `PLL_BL0` | `lpddr4_pll` | DDR clock from `ddr_clk_ref` | `142637` |
| `PLL_BL1` | `pll_inst1` | system/HDMI clock from 74.25 MHz | `142643` |
| `PLL_BL2` | `pll_inst4` | MIPI byte clock from `i_fb_clk` | `142649` |
| `JTAG_USER1` | `jtag_inst1` | existing video JTAG user tap | `142831` |
| `GPIOT_P_50` | `clk_25m` | 25 MHz PLL input, package pin L17 | `142849` |
| `GPIOL_32` | `clk_74p25m` | 74.25 MHz PLL input, package pin V3 | `142859` |
| `GPIOL_25` | `ddr_clk_ref` | 100 MHz DDR PLL input, package pin U4 | `142908` |

The official Hard SoC system PLL accepts only `PLL_BL0`, `PLL_BL1`, or
`PLL_BL2`. All are live video dependencies. `JTAG_USER1` is also occupied,
though a future SoC can use `JTAG_USER2` instead. The clock GPIO overlap is
real: the generated SoC must not create duplicate GPIO definitions for the
existing video clock pads.

## Decision

There is no legal small edit that adds the Hard SoC to the current verified
video build. Directly merging a generated SoC `.peri.xml`, editing generated
RTL, or reusing the official eMMC `soc.h` would create an invalid design and
is prohibited.

The current verified camera bitstream remains the rollback baseline.

## Candidate A: Preserve Video, Continue CPU Software

Keep the current video project unchanged. Continue migrating the CPU F1
application, UART command parser, PLACE/ABANDON adapter, single-camera
contract, and Host/RISC-V standalone builds under `competition_project_single_camera/cpu/`.

This advances software and test coverage but does not prove a board CPU UART
or CPU-to-FPGA register path. It is the lowest-risk route for the camera
baseline.

## Candidate B: Separate GUI Replan Experiment

Create a timestamped copy of the D-drive video project solely for a GUI
resource experiment. Interface Designer must attempt to legally move exactly
one video `PLL_BL*` dependency while preserving its source, output frequency,
reset behavior, and downstream DDR/CSI/HDMI assignments; the released
`PLL_BL*` is then reserved for a minimal Hard SoC. The SoC must use
`JTAG_USER2`, UART0 only, and no APB/UART2/GPIO business peripherals.

No source is merged back, no CPU firmware is written, and no board burn occurs
until Codex reviews the generated `.peri.xml`, wrapper, `soc.h`, linker,
resource diff, and Interface Designer warnings. Map/PNR and a camera HDMI
regression follow only after review approval.

This is the only currently known route to an on-board Hard SoC while retaining
the single-camera architecture, but its feasibility is **NOT VERIFIED** and it
can fail if a video PLL cannot be relocated by the tool.

## Recommended Order

Proceed with Candidate A immediately because it has no effect on the verified
camera pipeline. Start Candidate B only after explicit approval, because it
changes clock/periphery resources and requires a full board video regression.

## Not Allowed At This Gate

- Move or delete a live PLL in the verified project.
- Change `JTAG_USER1` in the verified project.
- Hand-edit `mem_test.peri.xml`, generated SoC RTL, constraints, or `soc.h`.
- Configure UART2, connect the arm, or send any arm command.
- Treat Host/Mock CPU tests as a board CPU result.
