# I0 UART1 Goal 4 Two-Phase Execution Review Packet

Status: `READY FOR QZS RE-REVIEW / BASE 9949e6ed ONLY / HARDWARE NOT AUTHORIZED`.

## Provenance And Scope

- Patch base: `9949e6ed737f25db82111cc38250dfc15bdb54c9`.
- QZS temporary ownership registration:
  `a222ea64653a2232945342faacfb53a06ce50e42`, covering
  `competition_project_single_camera/tools/run_i0_uart1_execution_chain.ps1`.
- WSC contract source, consumed read-only from a clean checkout:
  `48548f47dfa5964b13aed7edf3b3e9da6f6583a2`.
- Fixed design source: `6effdc3685d696cb4d33f3fbb1c449729ed72e33`.

This is one minimal follow-up on the same libaoxun branch. It does not merge,
rebase, cherry-pick, or migrate the histories of `a840f08`, `5a61c4c`,
`48548f4`, or `a222ea6`. QZS should review only the new commit on this base
and must not merge `69a1030e`.

Changed files remain within the six-file temporary scope: runner, static
verifier, authoritative manifest, target cfg, operation card, and this Packet.
The inherited cfg carries the required target correction:
`CPUTAPID 0x006A0A79 -> 0x006A0EF3`. `0x006A0EF3` is the correct TJ375N529
outer TAP ID and must not be changed back to the Ti375 default `0x006A0A79`.
User `.gitignore` is untouched.

`tools/i0_uart1_execution_manifest.json` is the single authoritative hash
inventory. It binds every changed file except itself. This Packet neither
stores the manifest hash nor is hashed by a second Packet, so there is no
Packet/manifest circular dependency. A future external approval record may
bind the final manifest hash without becoming a repository authorization token.

## Implemented Control Chain

Live mode represents one run ID and one OpenOCD process. It accepts only
`Scenario=run`; `Scenario=all`, automatic retry, TAP/cable changes, UART0
fallback, and generic token authorization fail before external action.

Before creating a live execution log, opening serial, or starting OpenOCD/GDB,
the runner requires an external RunDir, confirms the checkout HEAD equals
`approval_record.approved_commit`, requires empty
`git status --porcelain --untracked-files=all`, and hashes every
`manifest.files` member including the runner itself. Before OpenOCD starts it
then checks the bitstream, Hello ELF, probe ELF,
same-batch `soc.h`, FTDI cfg, target cfg, and all three WSC contract file hashes.
It verifies the WSC checkout is clean at the fixed SHA, then parses entry PC,
halt PC, timeout, RAM addresses/values, probe ELF hash, and `soc.h` hash from
that source. It does not keep a second driftable copy of those constants. After
launch it waits for the fixed GDB-listener readiness line; exit or bounded
readiness timeout fails without retry.

The target argument path remains exactly:

```text
-f <approved FTDI cfg> -f <i0_uart1_cleanlf_user2.cfg>
riscv use_bscan_tunnel 6 1
riscv set_bscan_tunnel_ir 0x09
```

The same target cfg fixes the outer FPGA TAP expected ID to the TJ375N529
identity `0x006A0EF3`; the Efinity FTDI template's `0x006A0A79` default is for
Ti375 and is rejected by the verifier.

`OPENOCD_ARGUMENT_FLOW=PASS` means the future process consumes those exact
files and arguments. It does not claim USER2 board PASS.

The full sequence is uniquely bound Type-C UART1 capture, Hello load and exact
post-load PC gate, `HELLO_RESUME_COUNT=1`, exact three-line Hello, one
printable non-CR/LF TX byte and same-byte echo on a new independent 2 s echo
deadline (not the 10 s three-line Hello deadline), confirmed halt, APB probe load
and exact post-load PC gate, then `APB_RESUME_COUNT=1`. The APB phase is logged
only after `HELLO_ECHO=PASS`. Whole-chain `RETRY_COUNT=0`.

After Hello resume, a timeout, line mismatch, echo mismatch, or RSP exception
issues exactly one Ctrl-C and waits a finite halt reply. The log records the
halt PC/reason; no reply records
`HELLO_HALT_UNCONFIRMED RAM_READ_COUNT=0 APB_RESUME_COUNT=0 RETRY_COUNT=0`.
Either result prohibits APB and retry. The APB watchdog is the parsed WSC `1000 ms` value. Timeout issues one active
Ctrl-C halt. `TIMEOUT_HALT_UNCONFIRMED`, timeout, trap, wrong PC, or wrong
reason reads zero RAM words. Only confirmed `BREAKPOINT` at `0xF90000C4` reads
the four WSC RAM evidence words.

RSP fixtures cover ACK, NACK without retry, checksum ACK/NACK generation,
half/sticky/fragmented packets, and asynchronous stop replies. Additional
negative fixtures cover the fixed Hello transcript, wrong echo byte, wrong
post-load PC false positive, wrong artifact hash, and wrong approval tuple.
The live-preflight negatives cover dirty runner, dirty cfg, wrong HEAD, and a
manifest-file hash mismatch; all report `EXTERNAL_PROCESS_START_COUNT=0`.

UART1 identity requires one exact `VID/PID/serial/instance` match. COM17,
CH340, J44/UART0, and programmer identities are explicitly rejected. The same
orchestrator atomically creates both persistent phase markers and the complete
execution log.

```text
HELLO_RESUME_COUNT=1
APB_RESUME_COUNT=1
RETRY_COUNT=0
```

## Original 82/21 Evidence Rerun

The six restored original evidence files under
`C:\cicc_i0_uart1_stage_20260719_v4` remain byte-for-byte bound as follows:

| File | SHA-256 |
|---|---|
| `efinity_console.log` | `4DD0C40BAF660B440B08B6668617D37ED01476B192663155385CF24F421655B4` |
| `uart1_hello_build.log` | `83CAAC75485C8C0098DFE2379C9CA329CE75429C1257F88A29E38CF4EF08B939` |
| `uart1_hello_readelf.txt` | `43C7825F6FEAC3086E17B57772D2469BD53EA9831528001DBC8771CF2C3277F3` |
| `uart1_hello_undefined.txt` | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| `preflight.txt` | `9DC15B0661CF43C3C2877946A989D89D53751F58E5F5B70A53BF7A2964F53CEF` |
| `postflight.txt` | `A502C8C5CD0437E379C957838FAD61FC0F48FEFE1D236C3DBBC97E9DCCE03D9F` |

Exact verifier command:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\cicc_i0_uart1_evidence_lf_20260719_v2\competition_project_single_camera\embedded_sw\uart1_hello_onchip\verify_i0_uart1_build_evidence.ps1 -DesignRoot C:\cicc_i0_uart1_design_lf_20260719_v4 -EvidenceRoot C:\cicc_i0_uart1_stage_20260719_v4 -OutflowRoot C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera\outflow_i0_uart1_20260719_cleanlf_v4 -WorkRoot C:\cicc_i0_uart1_design_lf_20260719_v4\competition_project_single_camera\work_i0_uart1_20260719_cleanlf_v4 -EfinityRoot D:\Efinity\2025.2 -RiscvToolchainBin D:\Efinity\efinity-riscv-ide-2025.2\toolchain\bin -MakePath D:\Efinity\efinity-riscv-ide-2025.2\build_tools\bin\make.exe
```

Recorded result:

```text
I0_UART1_BUILD_EVIDENCE=PASS batch=I0_UART1_20260719_CLEAN_LF_FINAL design_sha=6effdc3685d696cb4d33f3fbb1c449729ed72e33 inputs=82 artifacts=21
exit_code=0
```

This proves offline build evidence only. It does not prove USER2, UART1, APB,
or any board gate.

## Validation And Safety

The submission validation is limited to mock/offline fixtures, the clean WSC
contract verifier, WSC G2 Host evidence with an explicit external `-RunDir`,
the original 82/21 evidence verifier, PowerShell parsing, manifest hashes,
EOL, `git diff --check`, temporary scope, and dangerous-route scans.

```text
HARDWARE_ACTIONS=NONE
USER2=NOT_VERIFIED
UART1=NOT_VERIFIED
APB=NOT_VERIFIED
```

No Efinity Programmer, OpenOCD, GDB, JTAG, UART, APB, wiring, Flash, DDR,
UART2/J52, or mechanical-arm session was started.

OpenOCDExe/GdbExe normalization, version, and SHA-256 are intentionally not
yet approval-bound. This remains an explicit live-execution blocker; the runner
will not silently treat either executable as approved until a later minimal
reviewed manifest and approval extension binds both paths and hashes.
