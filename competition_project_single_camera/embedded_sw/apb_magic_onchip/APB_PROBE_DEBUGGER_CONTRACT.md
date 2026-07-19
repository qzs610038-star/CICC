# Goal 4 I2 APB Probe Debugger Contract

> Status: `STATIC CONTRACT ONLY / BOARD NOT AUTHORIZED / BOARD NOT VERIFIED`
>
> Patch baseline: `182fd6f5c4d628379760d6f4fc74e3b342e30083`.
> Handoff record: `fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c`.
> Design input: `a840f0869c11bab0915757d64c56a167f6d4f917`, read with
> `git show` only. This patch does not integrate that commit or its history.

This is a fail-closed contract for a future, separately approved board session.
It does not authorize Efinity Programmer, OpenOCD, GDB, JTAG, USER2, UART, APB,
wiring, or any other hardware action.

## Fixed Identity

| Item | Fixed value |
|---|---|
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |
| Probe ELF SHA-256 | `6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC` |
| ELF entry | `0xF9000000` |
| LOAD end | `0xF90008F0` |
| On-chip RAM | `0xF9000000..0xF9003FFF` |
| APB address | `0xE8100000 + 0x000` from the same-batch `soc.h` |
| CPU APB access | exactly one 32-bit `lw` |
| APB stores | zero |
| Deterministic halt PC | `0xF90000C4` |
| Run timeout | `1000 ms` |

No ELF, objdump, readelf, symbols, or other raw artifact is committed. The fixed
hashes and instruction counts remain evidence-host prerequisites.

## Success State Machine

After a separate Review Gate authorizes one session, the host must:

1. Arm UART capture and record `UART_CAPTURE_READY` before CPU resume.
2. Load the fixed ELF into on-chip RAM without implicit resume.
3. Verify the post-load gate `PC == 0xF9000000`.
4. Set one execution breakpoint at `0xF90000C4`.
5. Resume exactly once.
6. Within `1000 ms`, accept only halt reason `BREAKPOINT` and
   `PC == 0xF90000C4`.
7. Only after step 6 passes, read exactly four 32-bit on-chip RAM symbols.

| Symbol | RAM address | Required PASS value |
|---|---:|---:|
| `g_apb_probe_expected` | `0xF90000D0` | `0x375A0001` |
| `g_apb_probe_address` | `0xF90000D4` | `0xE8100000` |
| `g_apb_probe_status` | `0xF90000E4` | `0x50415353` |
| `g_apb_probe_observed` | `0xF90000E8` | `0x375A0001` |

These are RAM evidence reads, not APB transactions. The debugger must perform
zero direct APB reads and zero APB writes.

## Failure State Machine

For timeout, the host issues one halt at `1000 ms`, records elapsed time, PC, and
reason, then ends with `RAM_READ_COUNT=0`. It must not retry, resume, step, read
APB, write APB, or read any evidence symbol.

For trap, wrong PC, or wrong halt reason, the host records PC and reason and ends
immediately with `RAM_READ_COUNT=0`. It must not retry or inspect RAM/APB.

## Prohibited Operations

- No debugger read, watch, dump, poll, or monitor of
  `0xE8100000..0xE8100FFF`.
- No APB store, other offset, address scan, guessed address, retry, or step over
  the load.
- No second resume and no RAM evidence read before the success halt gate.
- No inference from Host PASS to USER2, PC execution, UART1, or APB board PASS.

## Runbook Acceptance Clauses

The consuming Runbook must retain the post-load PC gate, exactly one resume,
`UART_CAPTURE_READY` before resume, exactly one approved printable ASCII byte for
I1, disabled terminal automatic CR/LF append, timeout with no RAM read or retry,
and the statement that hardware actions remain unauthorized.

## Decision

The future session may report APB probe PASS only when fixed identities, entry
gate, single resume, timeout, halt reason, halt PC, and all four RAM values pass
in one approved session. Any missing field is `EVIDENCE_INCOMPLETE`; Goal 4 I2
remains blocked at the board level.
