# Type-C UART1 Video Debug Technical Plan

> Date: 2026-07-09
> Scope: planning only. This document does not imply RTL or CPU code has been modified.
> Target: use the board Type-C UART1 as a PC-facing debug console for video-link troubleshooting.

## 1. Purpose

Build a lightweight debug channel:

```text
PC serial terminal
  <-> board Type-C UART1, 115200 8N1
  <-> on-board CPU firmware
  <-> FPGA video/debug status registers
  <-> CSI / DDR / framebuffer / HDMI debug state
```

The first use case is video-link troubleshooting: confirm whether the camera input, DDR/framebuffer path, CDC bridge, and HDMI output path are alive, stable, underflowing, or stuck in fallback.

This is not a myCobot control link. Arm control remains a separate future path:

```text
on-board CPU
  <-> UART2
  <-> myCobot 280
```

## 2. Confirmed Boundaries

- Type-C UART1 is the PC debug and observability port.
- UART1 first-version serial format: `115200 8N1`.
- myCobot control is out of scope for this plan.
- myCobot should use an independent UART2 path and `1000000` baud when that phase starts.
- The debug console may print arm-related state in the future, but must not directly trigger motion in this plan.
- FPGA RTL should expose compact status/register information; command parsing and human-readable formatting should live in CPU firmware.
- Final SoC base addresses must come from the current Efinity-generated `soc.h`, not from old official examples or placeholder `bsp.h` values.

## 3. Source References

Primary repo references:

- `AGENTS.md`: global project rules, serial safety boundary, Type-C/JTAG/UART2 distinction.
- `CURRENT_STATE.md`: current route index and deprecated pure-FPGA control routes.
- `final_project/integration/fpga_cpu_interface.md`: FPGA/CPU register and UART separation guidance.
- `final_project/integration/register_map.md`: first register-map draft and valid/ack concepts.
- `final_project/integration/bringup_checklist.md`: CPU hello, UART print, register read/write, OSD writeback bring-up sequence.
- `final_project/integration/io_pin_map.md`: target place to record final Type-C UART1 and UART2 pin mapping.
- `final_project/docs/architecture/riscv_official_examples_integration.md`: official RISC-V example lessons and `soc.h` address-source warning.
- `final_project/docs/debug_sessions/hdmi_stripe_debug_20260707.md`: current video-link debug context and HDMI/CDC probe workflow.

Hardware mapping to carry forward:

- Type-C UART1: PC debug channel.
- UART1 RXD/TXD are reported from official hardware review as `GPIOR_96` / `GPIOR_100`.
- UART2 is reserved for external peripherals such as myCobot; reported pins are `GPIOR_97` / `GPIOR_101`.
- These pin details must be rechecked against the official schematic and then recorded in `final_project/integration/io_pin_map.md` before implementation.

## 4. Recommended Architecture

Use a CPU-first console.

Do not start by writing a pure FPGA UART module. The CPU is better suited for:

- command parsing,
- printable status formatting,
- register polling,
- timestamp/heartbeat output,
- future parameter debug commands,
- avoiding extra RTL risk while the video chain is still under bring-up.

FPGA responsibilities should stay narrow:

- expose video-link state as read-only debug registers,
- expose a small number of clearable error/latch bits if needed,
- avoid receiving complex ASCII commands directly,
- avoid coupling debug UART traffic into video timing logic.

## 5. Phase Plan

### Phase 0: Pre-Implementation Checks

Before code changes:

1. Confirm PC sees the Type-C UART1 COM port.
2. Confirm FTDI/JTAG interfaces and UART interface drivers are not conflicting.
3. Export or locate the current Efinity-generated `soc.h`.
4. Record UART1 base address, CLINT clock, APB/AXI user windows, linker, and OpenOCD/debug profile.
5. Update or prepare to update `io_pin_map.md` with confirmed UART1/UART2 mapping.

Expected output:

- A short bring-up note with COM port number, baud rate, driver state, and current `soc.h` summary.

### Phase 1: UART1 Console Alive

Goal: prove PC <-> Type-C UART1 <-> CPU works independently of video logic.

Minimal CPU console behavior:

- print boot banner,
- print firmware version/build marker,
- print UART1 base and baud rate,
- print heartbeat every second,
- receive ASCII line commands ending with `\r`, `\n`, or `\r\n`,
- support a small fixed command set.

Initial command set:

| Command | Purpose | Expected response |
|---|---|---|
| `help` | list commands | one-line command summary |
| `ping` | liveness check | `pong` |
| `ver` | firmware/build identity | version string |
| `echo <text>` | RX/TX check | repeats payload |
| `status` | CPU/debug status | alive, counters, last command, error code |

Implementation notes:

- Keep the first parser line-buffered and bounded, for example 64 or 128 bytes.
- Count RX bytes, TX bytes, malformed commands, and overflow events.
- If input line overflows, discard until newline and report an overflow counter.
- Do not add interrupt dependency in the first version unless the SoC UART driver already has a proven interrupt path.
- Polling is acceptable for Phase 1.

Phase 1 acceptance:

- PC terminal receives boot banner.
- `ping` returns `pong`.
- `echo abc` returns `abc`.
- `status` shows CPU alive and monotonic heartbeat/counters.
- HDMI/video behavior is unchanged by enabling UART output.

### Phase 2: Video Status Readout

Goal: use UART1 to make the video chain observable.

CPU reads FPGA debug/status registers and prints a compact interpretation.

Recommended first status groups:

| Group | Example fields |
|---|---|
| Identity | `REG_MAGIC`, `REG_VERSION` |
| Frame | `REG_FRAME_ID`, frame-valid/latch bits |
| DDR | config done, read/write activity, read gap, underflow |
| CSI | channel 0/1 frame seen, line seen, RAW10/4ppc format flag |
| Framebuffer | channel 0/1 frame ready, FIFO underflow, write done, read available |
| CDC/HDMI | bridge active, bridge underflow, input stable, HDMI video ready, fallback state |
| Errors | sticky error flags and clearable debug latches |

Suggested commands:

| Command | Purpose |
|---|---|
| `status` | short human-readable summary |
| `dump video` | detailed video register dump |
| `read <offset>` | read one debug register |
| `watch video <ms>` | periodically print compact video status |
| `log on` / `log off` | enable/disable periodic log output |

Example `status` shape:

```text
cpu=alive hb=12345
video: ddr=ok csi0=frame_seen csi1=no_frame fb0=ready fb1=underflow hdmi=input_stable
err=0x00000004 last_cmd=status
```

Phase 2 acceptance:

- `status` can distinguish at least DDR configured, CSI frame seen, framebuffer ready/underflow, and HDMI ready/fallback.
- `dump video` prints raw register values and decoded meanings.
- `watch video 1000` can run for several minutes without flooding or blocking the CPU main loop.
- Debug output does not materially disturb HDMI bring-up observations.

### Phase 3: Controlled Debug Actions

Only after Phase 2 is stable:

- add `clear err` for clearable debug latches,
- add `set loglevel <n>` for console verbosity,
- optionally add read-only snapshots of current selected HDMI channel and debug LED-equivalent state.

Avoid in this phase:

- changing MIPI timing,
- changing DDR/framebuffer scheduling,
- toggling reset paths,
- triggering myCobot motion,
- using UART commands as a hidden required part of the normal video pipeline.

Any command that changes video behavior must be reviewed separately because it can mask or introduce timing bugs.

## 6. Proposed File Impact When Implementation Is Approved

CPU-side likely files:

- `final_project/cpu/app/include/bsp.h`: add or correct UART1 base only after `soc.h` confirmation.
- `final_project/cpu/app/src/main.c`: call console init/tick loop.
- `final_project/cpu/app/include/uart_console.h`: console public API.
- `final_project/cpu/app/src/uart_console.c`: RX line buffer, parser, command dispatch.
- `final_project/cpu/app/include/video_debug_cli.h`: video-debug command declarations.
- `final_project/cpu/app/src/video_debug_cli.c`: status formatting and register decode.

FPGA-side later files:

- `final_project/fpga/rtl/.../video_debug_regs.v`: only if current debug status is not already exposed through an APB/AXI register window.
- `final_project/fpga/rtl/top/top.v`: connect debug signals only after register-window design is reviewed.
- `final_project/fpga/efinity/mem_test.xml`: include new RTL only if new RTL is added.
- `final_project/fpga/efinity/constrain.sdc` / `.peri.xml`: only if pin/peripheral changes are required; these changes trigger a stronger review gate.

Documentation:

- `final_project/integration/io_pin_map.md`: record confirmed UART1/UART2 mapping.
- `final_project/docs/bringup/typec_uart1_video_debug_bringup.md`: record COM port, commands tested, and observed output.
- `final_project/docs/debug_sessions/hdmi_stripe_debug_20260707.md`: link any new video-debug findings if they affect ongoing HDMI debug.

## 7. Register-Exposure Strategy

Do not make UART1 read internal RTL wires directly.

Preferred sequence:

1. Consolidate debug signals into a register block.
2. Sample or synchronize cross-clock signals safely before exposing them.
3. CPU polls the register block.
4. UART1 prints decoded values.

For cross-clock or pulse signals:

- use sticky bits for "event happened",
- use current-state bits for live state,
- use counters for high-frequency activity,
- provide explicit clear bits for sticky flags where useful,
- avoid relying on one-cycle pulses visible only in the CPU clock domain.

## 8. Safety Rules

- UART1 commands must not move the myCobot.
- UART1 commands must not power on/off, release, or emergency-stop the myCobot.
- Heartbeat timeout must not trigger arm motion or arm emergency behavior.
- Any future command that can indirectly start a grab must require a separate review, explicit user confirmation, and safety interlocks.
- PC and Type-C UART remain development/debug aids, not part of the official closed-loop dependency.

## 9. Open Questions Before Coding

These should be resolved before implementation:

1. What is the exact UART1 base address in the generated `soc.h` for the current Efinity design?
2. Is UART1 already enabled in the current SoC/peripheral configuration, or must the Efinity SoC config be changed?
3. Which CPU clock value should be used for UART divider calculation in this exact generated SoC?
4. Which APB/AXI user window is free for video debug registers?
5. Which current video debug signals are already available in the CPU-visible domain, and which require CDC/sticky conversion?
6. Should the first implementation live only in the formal C-drive repo tree, or also be mirrored to the active `D:\final_project` build/flash tree after review?

## 10. Suggested First Implementation Checklist

1. Run handoff health check.
2. Confirm `soc.h` and UART1 address.
3. Add UART1 low-level init/send/receive helpers.
4. Add line-buffered console with `help/ping/ver/echo/status`.
5. Build CPU firmware.
6. Load FPGA bitstream and CPU program.
7. Open PC serial terminal at `115200 8N1`.
8. Verify Phase 1 commands.
9. Record output and COM port in bring-up note.
10. Only then start Phase 2 video register readout.

## 11. Review Decision Needed

Approve this plan before implementation.

Recommended approval scope for the next coding step:

- implement Phase 1 only,
- do not modify FPGA RTL,
- do not modify Efinity `.peri.xml` or pin constraints,
- do not touch myCobot UART2 code,
- produce a short bring-up log after testing or after build-only verification if hardware is unavailable.
