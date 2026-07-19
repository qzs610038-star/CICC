# I0 UART1 Goal 4 Execution Chain Review Packet

Status: `READY FOR QZS STATIC REVIEW / HARDWARE NOT AUTHORIZED / BOARD NOT VERIFIED`.

## Provenance And Ownership

- Patch base: `2d713b80a41185e472837abaec3a10c01383c70f`.
- QZS temporary-ownership authorization:
  `a222ea64653a2232945342faacfb53a06ce50e42`, file
  `docs/agent_context/GOAL4_TEMPORARY_OWNERSHIP_REGISTER_20260719.md`.
- WSC contract source: `48548f47dfa5964b13aed7edf3b3e9da6f6583a2`.
- Fixed design source: `6effdc3685d696cb4d33f3fbb1c449729ed72e33`.

This is one minimal follow-up on the existing libaoxun personal branch. It does
not merge, rebase, cherry-pick, or copy the histories of `a840f08`, `5a61c4c`,
`48548f4`, or `a222ea6`. The qzs and WSC commits are read-only provenance.

## Changed Files

- `tools/i0_uart1_cleanlf_user2.cfg`
- `tools/run_i0_uart1_execution_chain.ps1`
- `tools/verify_i0_uart1_execution_config.ps1`
- `tools/i0_uart1_execution_manifest.json`
- `docs/debug_sessions/I0_UART1_CLEANLF_USER2_OPERATION_CARD_20260719.md`
- this Review Packet

`tools/i0_uart1_execution_manifest.json` is the only authoritative file-hash
inventory. The Packet does not repeat file hashes or hash the manifest, so
there is no Packet/manifest circular dependency. The manifest binds every
modified runtime, verifier, operation-card, and Packet file except itself.

## OpenOCD Argument Flow

The target configuration now supplies the RISC-V target commands literally:

```text
riscv use_bscan_tunnel 6 1
riscv set_bscan_tunnel_ir 0x09
```

The orchestrator constructs the future process arguments in fixed order:

```text
-f <approved FTDI config> -f <i0_uart1_cleanlf_user2.cfg>
```

Offline fixtures verify that order and the literal target arguments. This is
`OPENOCD_ARGUMENT_FLOW=PASS`, not USER2 hardware proof. No OpenOCD process was
started during this patch; `USER2=NOT_VERIFIED`.

## Host State Machine

`run_i0_uart1_execution_chain.ps1` is the only authorized host orchestrator.
Before any external action, Live mode requires `Scenario=run` and rejects
`Scenario=all`, fixture outcome labels, a missing approval token, and missing
explicit tool/config/ELF paths. One Live invocation represents one run and owns
one global resume count.

The same runner implements and fixture-tests:

- RSP ACK and NACK handling with no retry after NACK.
- RSP checksum validation and `+`/`-` response generation.
- persistent parser state for half packets, sticky packets, TCP fragments, and
  asynchronous stop replies.
- a `1000 ms` host watchdog followed by one active Ctrl-C halt request.
- `TIMEOUT_HALT_UNCONFIRMED` when no halt reply follows; this path reads zero
  RAM words and performs zero retries.
- four RAM evidence reads only for confirmed `BREAKPOINT` at `0xF90000C4`
  before timeout. Timeout, trap, wrong reason, wrong PC, and unconfirmed halt
  all require `RAM_READ_COUNT=0`.

## UART1 Identity And Evidence

Live UART1 requires one exact allowlist match containing VID, PID, serial, and
full PnP instance. Zero or multiple matches fail closed. COM17, CH340, and a
friendly name identifying J44/UART0/programmer/downloader/burner are rejected.
The currently connected J44/UART0 programmer is not a UART1 capture candidate.

The orchestrator creates the capture-ready marker, persistent
`RESUME_ONCE.marker`, UART byte log, and execution log under one run ID. The
offline fixtures verify:

```text
CAPTURE_READY_TIME < RESUME_ONCE_TIME
RESUME_COUNT=1
```

Logs contain the run ID, fixed SHAs, exact PnP identity, byte-level RX/TX
timestamps, final RX/TX counts, halt state, RAM-read count, retry count, and
final result.

## Offline Validation

- Runner fixtures: success, timeout, trap, wrong PC, wrong reason, and halt
  unconfirmed all pass their expected gates.
- Success reads four RAM words; every failure reads zero.
- WSC contract verifier is executed from a clean checkout at `48548f4`.
- WSC G2 host evidence is executed with an explicit external `-RunDir`.
- The original UART1 build verifier is executed from the `72cc281` evidence
  checkout with the original roots and reports `inputs=82 artifacts=21`, exit
  zero.
- `git diff --check`, temporary scope, dangerous-route scan, manifest hashes,
  and EOL policy pass. PowerShell is CRLF; GDB and CFG are LF.

Recorded WSC Host detail: the first automatic-compiler run selected GCC and
failed closed because the fixed WSC test uses MSVC `fopen_s`. The retry used the
script's public `-CompilerPreference Msvc` parameter with a new external
`-RunDir`, then passed `single_camera_runtime: 648/648` with exit zero. No WSC
file was changed.

## Safety And Remaining Boundary

No Efinity Programmer, OpenOCD, GDB, JTAG, USER2, UART, APB, wiring, Flash,
DDR, UART2/J52, or mechanical-arm session was started.

```text
HARDWARE_ACTIONS=NONE
USER2=NOT_VERIFIED
UART1=NOT_VERIFIED
APB=NOT_VERIFIED
```
