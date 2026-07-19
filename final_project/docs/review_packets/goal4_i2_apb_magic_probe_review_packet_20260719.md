# Goal 4 I2 APB MAGIC Probe Review Packet

> Status: `READY FOR QZS/CODEX STATIC REVIEW / BOARD NOT AUTHORIZED / BOARD NOT VERIFIED`
>
> `HARDWARE_ACTIONS=NONE`. This Packet covers a static evidence contract and Host
> state-machine verifier only.

## Goal And Conclusion

The patch fixes the future debugger boundary for one CPU-issued 32-bit read of
`IO_APB_SLAVE_0_INPUT + 0x000`. Four on-chip RAM symbols may be read only after
the entry, single-resume, timeout, halt-reason, and halt-PC gates pass. Timeout,
trap, wrong PC, and wrong halt reason fail closed with `RAM_READ_COUNT=0`.

Current conclusion: `STATIC CONTRACT AND HOST MODEL PASS`. USER2, CPU execution,
UART1, APB access, and the MAGIC result remain `NOT VERIFIED`.

## Provenance And Scope

| Purpose | Fixed identity |
|---|---|
| Patch baseline | `182fd6f5c4d628379760d6f4fc74e3b342e30083` |
| Remote handoff | `fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c` |
| Temporary ownership record | `docs/agent_context/GOAL4_TEMPORARY_OWNERSHIP_REGISTER_20260719.md` at the handoff SHA |
| Read-only design input | `a840f0869c11bab0915757d64c56a167f6d4f917` |

The branch was created clean from the patch baseline. No merge, rebase, or
cherry-pick of the design input was performed. Only the four authorized paths
are changed.

## Fixed Inputs

| Item | Value |
|---|---|
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |
| Probe ELF SHA-256 | `6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC` |
| Entry / LOAD end | `0xF9000000` / `0xF90008F0` |
| RAM | `0xF9000000..0xF9003FFF` |
| APB | `0xE8100000 + 0x000`, one `lw`, zero writes |
| Halt / timeout | `0xF90000C4` / `1000 ms` |

No ELF, objdump, readelf, symbols, undefined listing, Efinity output, local tool
path, license, or temporary database is committed. Raw artifact checks remain a
future evidence-host prerequisite.

## Changed Files

- `competition_project_single_camera/embedded_sw/apb_magic_onchip/APB_PROBE_DEBUGGER_CONTRACT.md`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/artifacts/apb_magic_onchip.contract.txt`
- `competition_project_single_camera/embedded_sw/apb_magic_onchip/verify_apb_probe_contract.ps1`
- `final_project/docs/review_packets/goal4_i2_apb_magic_probe_review_packet_20260719.md`

## Verification Design

The verifier parses exact key/value data and executes an in-memory Host gate
model; no fixture file is created. It covers success, timeout, trap, wrong PC,
and wrong halt reason. Every scenario asserts one resume, zero retries, zero
debugger APB reads, and zero APB writes. Only success permits four RAM reads;
every failure permits zero. This is state-transition verification, not a
string-only PASS check.

The consuming Runbook must retain the post-load PC gate, one resume,
`UART_CAPTURE_READY` before resume, one approved printable ASCII byte for I1,
disabled terminal CR/LF append, and timeout with no RAM read or retry. It is not
modified because it is outside the temporary four-file scope.

## Impact And Safety

- No RTL, XML, SDC, IP, BSP, CPU business source, clock, reset, CDC, AXI, video,
  OSD, UART, or myCobot file changes.
- `ARM_ENABLED=0`; UART2/J52 and the mechanical arm remain disconnected.
- No Programmer, OpenOCD, GDB, serial, JTAG, USER2, APB, wiring, or board action
  was executed.

## Required Validation

Record raw output and exit codes for `git diff --check`, the contract verifier,
`run_g2_host_evidence.ps1`, and `run_single_camera_classifier_host.ps1`.
Expected Host totals are `G2 runtime=648/648` and `classifier=54/54`. Host PASS
does not prove USER2, PC, UART1, or APB board behavior.

Actual static and Host results:

```text
verify_apb_probe_contract.ps1 exit=0
APB_PROBE_CONTRACT=PASS
NEGATIVE_SUCCESS=PASS RAM_READ_COUNT=4
NEGATIVE_TIMEOUT=PASS RAM_READ_COUNT=0 host_halt_count=1
NEGATIVE_TRAP=PASS RAM_READ_COUNT=0
NEGATIVE_WRONG_PC=PASS RAM_READ_COUNT=0
NEGATIVE_WRONG_HALT_REASON=PASS RAM_READ_COUNT=0
STATIC_APB=PASS main_lw_count=1 apb_write_count=0
HALT_CONTRACT=PASS halt_pc=0xF90000C4 timeout_ms=1000
HARDWARE_ACTIONS=NONE

run_g2_host_evidence.ps1 exit=1 when invoked exactly without parameters
reason=mandatory RunDir parameter missing in baseline 182fd6f

run_g2_host_evidence.ps1 -RunDir D:\cbm_test_index\goal4-wsc-contract-20260719 exit=0
single_camera_runtime: 648/648 passed
VALIDATION_PASS: offline bundle

run_single_camera_classifier_host.ps1 exit=0
single_camera_classifier: 54/54 passed
```

The explicit G2 `RunDir` is outside the repository and is not committed. The
missing-parameter result is retained because the task's literal bare command is
not executable against this fixed baseline; the parameterized run is the actual
Host validation.

Read-only static artifact audit against the design evidence worktree:

```text
apb_magic_onchip.elf SHA-256=6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC
soc.h SHA-256=25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B
readelf entry=0xF9000000
readelf LOAD=0xF9000000 filesz=0x000E2 memsz=0x008F0
objdump f90000a4=lui a5,0xe8100
objdump f90000a8=lw a4,0(a5)
objdump f90000c4=j f90000c4
```

Governance contradiction: `tools/team_scope_check.ps1 -Role wsc` exit=1 because
its fixed policy predates the temporary ownership register and rejects all four
newly authorized paths. The handoff register at `fd3fc08` explicitly grants these
exact paths. Updating the checker would be a fifth unauthorized file, so this
patch records the contradiction and leaves adjudication to qzs.

Any identity mismatch, timeout, trap, unexpected halt, or incomplete evidence
keeps Goal 4 I2 blocked. Do not retry, scan addresses, or write APB. Rollback is
removal of this four-file text-only patch; no hardware state exists to roll back.

Requested ruling: accept or reject this fail-closed contract for later
integration by libaoxun while keeping board actions behind a separate Review Gate.
