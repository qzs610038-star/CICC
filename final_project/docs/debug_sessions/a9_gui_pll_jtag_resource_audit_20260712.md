# A9 Efinity GUI PLL/JTAG Resource Audit

> Date: 2026-07-12
> Scope: `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga\efinity\mem_test.xml`
> Method: Read-only Efinity Interface Designer review, cross-checked against `mem_test.peri.xml` and top-level RTL.
> Status: PLL/JTAG resource conflict confirmed. GUI replanning, SoC integration, PNR, bitstream, and board behavior are NOT VERIFIED.

## Scope and Safety Boundary

- No changes were made to `D:\final_project\fpga`, repository RTL, `mem_test.xml`, `mem_test.peri.xml`, or `constrain.sdc`.
- No map, PNR, bitstream generation, download, programming, or board test was run.
- Do not hand-merge `.peri.xml`, modify generated RTL, or use only PLL/JTAG dropdown changes to attempt a SoC merge.

## PLL Resources Confirmed in GUI

| Resource | GUI instance | Reference clock | Main outputs | Downstream role | Conclusion |
|---|---|---|---|---|---|
| `PLL_BL0` | `lpddr4_pll` | `ddr_clk_ref`, 100 MHz, `GPIOL_25` | `pll_inst2_CLKOUT0`, `axi0_ACLK`, `lpddr4_pll_CLKOUT3` | LPDDR4 and AXI clocks | Cannot be released directly for a SoC system PLL |
| `PLL_BL1` | `pll_inst1` | `clk_74p25m`, 74.25 MHz, `GPIOL_32` | `pll_inst1_CLKOUT0`, `CLK_5M` | Video system and low-speed clocks | Cannot migrate without rebuilding its dependencies |
| `PLL_TR0` | `MIPI_TX_PLL` | `clk_25m`, 25 MHz, `GPIOT_P_50/GPIOT_PN_50` | `i_fb_clk`, MIPI TX fast/slow clocks | `i_fb_clk` feeds `PLL_BL2` | Forms a serial clock chain with `PLL_BL2` |
| `PLL_BL2` | `pll_inst4` | core `i_fb_clk`, 25 MHz | `mipi_clk` 100 MHz, `i_sysclk_div2`, HDMI fast/slow clocks | MIPI CSI receive and HDMI video | Cannot migrate or delete independently |

GUI parameters match `efinity/mem_test.peri.xml`:

- `PLL_BL0`: 100 MHz reference, 4800 MHz VCO, 2400 MHz PLL frequency.
- `PLL_BL1`: 74.25 MHz reference, 4752 MHz VCO, 594 MHz PLL frequency.
- `PLL_TR0`: 25 MHz reference, 4000 MHz VCO, 2000 MHz PLL frequency.
- `PLL_BL2`: 25 MHz `i_fb_clk` reference, 2800 MHz VCO/PLL frequency, 100 MHz `mipi_clk` output.

## JTAG Review

- GUI block `jtag_inst1` declares `JTAG_USER1`.
- It provides `TDI/TCK/TMS/SEL/DRCK/RESET/RUNTEST/CAPTURE/SHIFT/UPDATE` inputs and a `TDO` output.
- `rtl/top/top.v` declares the `jtag_inst1_*` ports, but drives `jtag_inst1_TDO` to `1'b0`; no active JTAG debug logic was found.
- `JTAG_USER1` is therefore declared/occupied but not the root cause of the SoC merge failure. A future regenerated SoC candidate should use `JTAG_USER2`.

## SoC Merge Decision

The current video project uses `PLL_BL0`, `PLL_BL1`, `PLL_BL2`, and `PLL_TR0`. The hard SoC system PLL accepts only `PLL_BL0/PLL_BL1/PLL_BL2`; all three are already in use.

Changing the SoC JTAG to `JTAG_USER2` does not resolve this PLL conflict. The earlier A2 review also found that the candidate SoC duplicates the video clock-pad resources `GPIOT_P_50` and `GPIOL_25`. Therefore: **do not directly merge a hard SoC into the current video project.**

## Evidence

- User-provided GUI screenshots on 2026-07-12 for the four PLL blocks and `JTAG_USER1`.
- `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga\efinity\mem_test.peri.xml`
  - `pll_inst1`: lines 216-248.
  - `lpddr4_pll`: lines 249-291.
  - `MIPI_TX_PLL`: lines 292-338.
  - `pll_inst4`: lines 339-370.
  - `jtag_inst1/JTAG_USER1`: lines 429-443.
- `C:\fpga_soc_isolated\tj375_video_soc_gui_a8\fpga\rtl\top\top.v`
  - JTAG ports: lines 63-82 and 116-117.
  - `jtag_inst1_TDO = 1'b0`: line 665.
  - `mipi_clk` MIPI RX usage: lines 1046, 1336, 1425.
- `final_project/docs/debug_sessions/a4_soc_video_resource_audit_20260712.md`.

## Next Steps and Prohibited Actions

1. Keep the A8 isolated project unchanged as the GUI audit baseline.
2. Continue CPU-only Host logic, interface contracts, and tests without `main.c` or board MMIO integration.
3. To resume the SoC route, first review a video-clock replanning proposal. Generate a new isolated candidate only through Efinity GUI/IP Manager.
4. Re-run resource intersection review, then map, PNR, bitstream, and board validation independently for the new candidate.

## NOT VERIFIED

- Whether Efinity provides a supported PLL replanning path that preserves the DDR/MIPI/HDMI chain.
- New SoC compatibility for clocks, resets, UART, JTAG, APB, and GPIO.
- Map/PNR, timing, bitstream, programming, CPU firmware, APB readback, OSD, camera input, and robot behavior.
