# Goal4-I2 APB Probe Debugger Contract

> Status: `STATIC CONTRACT ONLY / BOARD NOT AUTHORIZED / BOARD NOT VERIFIED`
>
> This contract is bound to probe commit `15908b32475f6ce80b645a728c25a5e7a2db749f`.
> It does not authorize Programmer, JTAG, UART, or board access.

## Fixed Identity

| Item | Fixed value |
|---|---|
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |
| Probe ELF SHA-256 | `6CB7E5E1CA5AF1B82FB99015925F13C2517B9332FD0FB0015DC1B4FDC3DFEAEC` |
| ELF entry | `0xF9000000` |
| LOAD | one segment, `0xF9000000..0xF90008F0` |
| APB access | exactly one `lw`, address `0xE8100000 + 0x000` |
| APB stores | zero |
| Deterministic halt PC | `0xF90000C4` |
| Run timeout | `1000 ms` |

## Exact Board Sequence After Separate Approval

The future operator must execute this sequence without inserting memory reads or
additional resume operations:

1. Verify the fixed `soc.h`, ELF, bitstream, and approval-window identities.
2. Load the fixed ELF into on-chip RAM only. Do not resume as part of load.
3. Halt and verify `PC == 0xF9000000`. Record PC, instruction bytes, and halt reason.
4. Set one execution breakpoint at `0xF90000C4` before resume.
5. Resume exactly once and wait at most `1000 ms` for a breakpoint halt.
6. PASS the run/halt gate only when halt reason is the configured breakpoint and
   `PC == 0xF90000C4`.
7. After the halt gate passes, read exactly these four 32-bit RAM symbols by symbol
   name or the fixed RAM addresses below:

| Symbol | RAM address | Required value |
|---|---:|---:|
| `g_apb_probe_expected` | `0xF90000D0` | `0x375A0001` |
| `g_apb_probe_address` | `0xF90000D4` | `0xE8100000` |
| `g_apb_probe_status` | `0xF90000E4` | `0x50415353` for PASS; `0x4641494C` for mismatch |
| `g_apb_probe_observed` | `0xF90000E8` | `0x375A0001` for PASS |

The four reads are evidence reads from on-chip RAM. They are not APB transactions.

## Prohibited Debugger Operations

- The debugger must not directly read, inspect, watch, dump, or poll `0xE8100000`.
- The debugger must not read any address in `0xE8100000..0xE8100FFF`.
- The debugger must not write any APB address or probe another offset.
- The debugger must not add a watchpoint on the APB window.
- The debugger must not resume more than once or step over the APB load.
- The debugger must not read the four RAM symbols before the deterministic halt gate.

Only the CPU instruction at `0xF90000A8` performs the APB transaction. The debugger
observes the CPU-produced RAM evidence after execution has stopped.

## Deterministic Halt And Timeout Evidence

The committed ELF converges both comparison outcomes to the self-loop at
`0xF90000C4` only after storing `g_apb_probe_observed` and
`g_apb_probe_status`. The breakpoint converts that self-loop into a deterministic
debugger stop.

Required run record:

```text
entry_pc=0xF9000000
breakpoint_pc=0xF90000C4
run_start_time=
halt_time=
elapsed_ms=
halt_reason=BREAKPOINT
halt_pc=0xF90000C4
timeout_ms=1000
timeout_triggered=false
```

If the breakpoint is not reached within `1000 ms`:

1. issue one debugger halt;
2. record `timeout_triggered=true`, elapsed time, halt reason, PC, and trap registers;
3. classify `APB_PROBE_RUN_TIMEOUT` or `APB_BUS_FAULT` as supported by evidence;
4. do not read the four RAM symbols;
5. do not resume, retry, step, or inspect the APB window.

## Result Decision

`Goal4-I2` can pass only when all of the following hold in one approved session:

- fixed identities match;
- entry/PC gate passes;
- deterministic halt gate passes within `1000 ms`;
- the four RAM symbol reads match the PASS values;
- the evidence log confirms zero debugger APB reads and zero APB writes.

Any missing field is `EVIDENCE_INCOMPLETE`. This contract does not change the
current Runbook status: Goal4-I2 remains `BLOCKED` pending QZS/Codex approval.
