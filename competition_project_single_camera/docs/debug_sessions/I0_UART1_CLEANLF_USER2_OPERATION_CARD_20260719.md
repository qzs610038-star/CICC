# I0 UART1 Goal 4 Future Operation Card

Status: `STATIC REVIEW ONLY / HARDWARE NOT AUTHORIZED / BOARD NOT VERIFIED`.

This card describes a future single controlled window. It was updated offline
on base `69a1030e1e72854a857fab147aa2c9cc8f0e6800`. QZS temporary ownership for
`tools/run_i0_uart1_execution_chain.ps1` is recorded at
`a222ea64653a2232945342faacfb53a06ce50e42`. The APB contract is consumed from
a clean checkout fixed at `48548f47dfa5964b13aed7edf3b3e9da6f6583a2`.

## Approval Record

Live mode has no generic token. A separately issued JSON record must bind one
tuple: final runner commit SHA, board ID, exact UART1
`VID/PID/serial/instance`, window ID and UTC start/end, stop strategy
`FIRST_FAILURE_STOP_NO_RETRY_CTRL_C_TIMEOUT`, authoritative manifest SHA-256,
and hashes for the bitstream, Hello ELF, probe ELF, `soc.h`, FTDI cfg, target
cfg, and the three WSC contract files. Any mismatch or non-unique PnP result
fails before OpenOCD starts.

COM17, CH340 `VID:PID=1A86:7523`, and names identifying J44, UART0, programmer,
downloader, or burner are prohibited. The programmer currently occupying J44
is not a UART1 capture candidate.

## Preflight And Process Boundary

The same orchestrator must hash all fixed artifacts and parse the constants
from the clean WSC checkout before starting one OpenOCD process. It waits for
the fixed GDB-listener readiness line and fails without retry if OpenOCD exits
or the bounded readiness interval expires. Its argument flow is exactly:

```text
-f <approved FTDI cfg> -f <fixed target cfg>
riscv use_bscan_tunnel 6 1
riscv set_bscan_tunnel_ir 0x09
```

The target cfg fixes the outer FPGA TAP identity to TJ375N529
`CPUTAPID=0x006A0EF3`. It must not inherit the Efinity FTDI template's Ti375
default `0x006A0A79`.

This is static argument-flow evidence only. It is not USER2 hardware proof.

## Single-Run Sequence

```text
one run ID / one OpenOCD process / RETRY_COUNT=0
  -> open uniquely approved Type-C UART1 at 115200 8N1, no CR/LF append
  -> persist CAPTURE_READY_TIME
  -> load fixed Hello ELF without resume
  -> parse exactly one HELLO_POST_LOAD_PC and require 0xF9000000
  -> atomically create HELLO_RESUME_ONCE.marker
  -> HELLO_RESUME_COUNT=1
  -> require the fixed three Hello lines in order
  -> transmit exactly one printable ASCII byte, excluding CR and LF
  -> require the same byte as echo
  -> confirmed halt; otherwise APB phase is prohibited
  -> load fixed APB probe ELF without resume
  -> parse exactly one APB_POST_LOAD_PC and require WSC entry 0xF9000000
  -> log APB_PHASE_STARTED_AFTER_HELLO_PASS=true
  -> atomically create APB_RESUME_ONCE.marker
  -> APB_RESUME_COUNT=1
  -> wait the WSC 1000 ms watchdog
  -> timeout: one active Ctrl-C halt request
  -> require BREAKPOINT and PC=0xF90000C4 before four RAM evidence reads
```

Both phase markers contain the run ID, ready time, resume time,
`RESUME_COUNT=1`, and `RETRY_COUNT=0`. The persistent execution log contains
the fixed SHAs, approval/window/board tuple, exact PnP identity, preflight
hashes, exact post-load PCs, byte-level RX/TX UTC timestamps, complete byte
counts, halt reason/PC, RAM-read count, and final state.

Timeout, trap, wrong PC, wrong reason, Hello failure, echo mismatch, artifact
hash mismatch, approval tuple mismatch, or unconfirmed halt ends fail-closed.
`TIMEOUT_HALT_UNCONFIRMED` permits no RAM read and no retry.

```text
HELLO_RESUME_COUNT=1
APB_RESUME_COUNT=1
RETRY_COUNT=0
HARDWARE_ACTIONS=NONE
USER2=NOT_VERIFIED
UART1=NOT_VERIFIED
APB=NOT_VERIFIED
```

No automatic retry, TAP/cable change, UART0 fallback, address scan, debugger
direct APB access, APB write, Flash, DDR, UART2/J52, or mechanical-arm route is
permitted.
