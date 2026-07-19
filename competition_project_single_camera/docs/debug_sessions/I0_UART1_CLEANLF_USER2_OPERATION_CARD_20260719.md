# I0 UART1 Clean-LF USER2 Operation Card

Status: `STATIC CONTRACT ONLY / HARDWARE NOT AUTHORIZED / BOARD NOT VERIFIED`.

## Fixed Inputs

| Item | Value |
| --- | --- |
| Patch base | `2d713b80a41185e472837abaec3a10c01383c70f` |
| QZS authorization | `a222ea64653a2232945342faacfb53a06ce50e42` |
| WSC contract | `48548f47dfa5964b13aed7edf3b3e9da6f6583a2` |
| Design SHA | `6effdc3685d696cb4d33f3fbb1c449729ed72e33` |
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| Bitstream SHA-256 | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` |
| Hello ELF SHA-256 | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |

The only authoritative tool/document hash inventory is
`tools/i0_uart1_execution_manifest.json`.

## Static Chain

The approved target config contains:

```text
riscv use_bscan_tunnel 6 1
riscv set_bscan_tunnel_ir 0x09
```

The future orchestrator must pass the approved FTDI config first and this
target config second. Static argument-flow PASS does not prove FPGA USER2
selection. `USER2=NOT_VERIFIED` until a separately approved board window.

## Future Window Preconditions

Do not execute this card without a new user approval tied to the exact files,
artifacts, board, and cabling. Before any external process starts:

1. The programmer must have exited and released JTAG.
2. J44/UART0 remains excluded from UART1 capture.
3. Type-C UART1 must uniquely match the approved VID/PID/serial/instance
   allowlist. COM17, CH340, J44/UART0, and programmer identities are rejected.
4. Live mode must use `Scenario=run`, one process, one run ID, and one resume.
   `Scenario=all`, fixture outcome labels, and automatic retry are prohibited.
5. Tool, config, ELF, and artifact identities must match the manifest and fixed
   batch before the window starts.

## Future Single-Run Sequence

```text
fixed volatile bitstream
  -> OpenOCD with approved FTDI cfg then fixed target cfg
  -> RAM-only probe load and entry PC gate
  -> exact Type-C UART1 identity bind
  -> CAPTURE_READY_TIME
  -> persistent RESUME_ONCE marker
  -> RESUME_ONCE_TIME and RESUME_COUNT=1
  -> one asynchronous stop wait with 1000 ms watchdog
  -> timeout: active halt request
  -> halt reason and PC gate
  -> success only: four fixed RAM evidence reads
  -> final persistent log and byte counts
```

Only confirmed `BREAKPOINT` at `0xF90000C4` before timeout permits RAM reads.
Timeout, trap, wrong reason, wrong PC, or `TIMEOUT_HALT_UNCONFIRMED` ends the
window with `RAM_READ_COUNT=0` and `RETRY_COUNT=0`.

## Prohibited Routes

No TAP/cable replacement, address scan, debugger APB read, APB write, UART0
fallback, G1/R0/M2 operation card, USER1, SoftTap, Flash, SPI, PROM, DDR,
UART2/J52, or mechanical-arm route is permitted.

```text
HARDWARE_ACTIONS=NONE
USER2=NOT_VERIFIED
UART1=NOT_VERIFIED
APB=NOT_VERIFIED
```
