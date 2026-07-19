# I0 UART1 Clean-LF USER2 Execution Configuration Review

Status: `READY FOR QZS FINAL STATIC REVIEW / HARDWARE NOT AUTHORIZED / BOARD NOT VERIFIED`.

## Scope And Provenance

This independent libaoxun patch starts at
`182fd6f5c4d628379760d6f4fc74e3b342e30083`. It consumes, without merge,
rebase, cherry-pick, or copied authorship, the WSC contract at
`48548f47dfa5964b13aed7edf3b3e9da6f6583a2`. The handoff register is
`fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c`.

Only the temporary ownership-register paths are changed. The WSC Packet path
`final_project/docs/review_packets/goal4_i2_apb_magic_probe_review_packet_20260719.md`
does not exist in the patch baseline, so this patch deliberately does not create
a second source. QZS must preserve and cite the WSC Packet from the fixed WSC
SHA during final integration.

## Static Evidence

| Item | Fixed value |
| --- | --- |
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| Bitstream SHA-256 | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` |
| Hello ELF SHA-256 | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |
| WSC probe ELF SHA-256 | `6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC` |
| USER2 outer IR | Titanium `01001`, `0x09`, width 5 |
| CPU TAP / tunnel | `0x006A0A79`; `6 1`; IR width `8` |
| RAM / valid PC range | `0xF9000000`; `0xF9000000..0xF9003FFF` |
| WSC halt / timeout | `0xF90000C4` / `1000 ms` |

The local Efinity installation provides the outer-IR source in
`debugger/bin/efx_dbg/jtag.py`: Titanium `USER2=01001`, and the shared JTAG
engine selects that mapped value through `write_ir()`. The Ti375C529 Hard SoC
`openocd/debug_ti.cfg` provides the CPU TAP and tunnel values. However, the
future direct OpenOCD configuration has no traceable invocation of Efinity's
outer-IR API. A variable set to `0x09` plus BSCAN commands is expressly not
enough evidence. `USER2_SELECTION_CHAIN=BLOCKED`; no USER2 session is
authorized. No USER1 or SoftTap route is selected.

## Contract Controls

- Initial halt, Hello ELF load, post-load halt, PC gate, `CAPTURE_READY`, and a
  single resume are explicit static controls.
- Type-C UART1 PnP identity must match an exact VID/PID/serial/instance
  allowlist entry; COM17 and CH340 are hard failures.
- The capture sends one printable `0x20..0x7E` byte only after Hello and accepts
  only that single-byte echo. CR/LF insertion is prohibited.
- WSC's CPU-owned probe permits one `lw`, zero APB stores, offset `0x000`, and
  expected value `0x375A0001`. Direct debugger APB reads are prohibited.
- No FPGA or CPU business source, RTL, XML, SDC, IP, BSP, interface freeze,
  `CURRENT_STATE.md`, `SESSION_HANDOFF.md`, UART2/J52, or myCobot file changes.

## Raw Build Evidence

The original verifier requires the clean-LF design root at
`C:\cicc_i0_uart1_design_lf_20260719_v4`, whose HEAD is
`6effdc3685d696cb4d33f3fbb1c449729ed72e33` and is clean. The required tool
paths match the manifest hashes. The actual evidence root containing all six
required build transcripts was not located in this static run, so the raw
82-input/21-artifact verifier cannot be claimed as PASS. No artifacts were
rebuilt, replaced, or committed.

## Required Static Checks

Run the raw build verifier with actual roots, this patch's execution verifier,
the WSC contract verifier from its fixed SHA, the final integration manifest
verifier, `git diff --check`, scope review, and the manifest/file/Packet SHA
comparison. Any mismatch blocks final hardware authorization.

## Actual Static Results

```text
WSC verify_apb_probe_contract.ps1 exit=0
APB_PROBE_CONTRACT=PASS
STATIC_APB=PASS main_lw_count=1 apb_write_count=0
HALT_CONTRACT=PASS halt_pc=0xF90000C4 timeout_ms=1000
HARDWARE_ACTIONS=NONE

verify_i0_uart1_execution_config.ps1 exit=2
USER2_SELECTION_CHAIN=BLOCKED
UART0_DEPENDENCY_COUNT=0
DIRECT_APB_READ_COUNT=0
APB_WRITE_COUNT=0
CAPTURE_BEFORE_RESUME=PASS
PNP_ALLOWLIST=PASS
SINGLE_PRINTABLE_BYTE_NO_CRLF=PASS
STATIC_EXECUTION_VERIFIER=BLOCKED
HARDWARE_ACTIONS=NONE

verify_i0_uart1_build_evidence.ps1 exit=1
missing evidence/efinity_console.log at the supplied actual clean-LF evidence root
BUILD_INPUTS=82
BUILD_ARTIFACTS=21

tools/verify_final_static_integration_manifest.ps1 exit=1
baseline checkout SHA mismatches: tools/interface_freeze_check.ps1,
tools/team_scope_check.ps1, tools/verify_final_static_integration_manifest.ps1

tools/team_scope_check.ps1 -Role libaoxun exit=1
the pre-existing policy rejects all ten temporary-register paths

git diff --check exit=0
temporary-register exact-path check: 10 changed, 0 outside scope
```

The first two blockers are not bypassable: a real outer-IR execution bridge must
be supplied for USER2, and the six missing raw evidence files must be recovered
at their actual evidence root. The final-manifest and role-policy failures are
baseline governance contradictions; this patch does not alter their non-owned
tools to obtain an artificial PASS.

## Safety State

`HARDWARE_ACTIONS=NONE`.

USER2, PC, UART1 Hello/echo, and APB MAGIC remain `NOT VERIFIED`; I3 remains
`BLOCKED_CONTRACT_NOT_FROZEN` pending QZS final static integration.
