# I0 UART1 Clean-LF USER2 Operation Card

Status: `STATIC CONTRACT ONLY / HARDWARE NOT AUTHORIZED / BOARD NOT VERIFIED`.

## Fixed Inputs

| Item | Value |
| --- | --- |
| Handoff SHA | `fd3fc0881d4e71338f1aa34f361cd498b7cd2d4c` |
| Patch baseline | `182fd6f5c4d628379760d6f4fc74e3b342e30083` |
| WSC contract SHA | `48548f47dfa5964b13aed7edf3b3e9da6f6583a2` |
| Batch | `I0_UART1_20260719_CLEAN_LF_FINAL` |
| Bitstream SHA-256 | `D05EFD4EC91FDB51F2997483BA7F4A3634C3E50FF1C65DD7239B41E19E2BC544` |
| Hello ELF SHA-256 | `919B291A2B980F02507D1591EFBE5D8E395DC53F4A480AE5A8C577D74CF5F7FA` |
| `soc.h` SHA-256 | `25BABB96D96E8FFE9F699A060A62ACEE7F0AC67DFAA31456EB6BCA88E62C982B` |

## Static Selection Chain

The local Efinity 2025.2 debugger API maps Titanium `USER2` to five-bit
`01001` (`0x09`). The packaged Ti375C529 Hard SoC `debug_ti.cfg` configures
`CPUTAPID=0x006A0A79`, BSCAN tunnel `6 1`, tunnel IR width `8`, and RAM work
area `0xF9000000`. `i0_uart1_cleanlf_user2.cfg` records that chain and begins
with an initial halt. `i0_uart1_cleanlf_ram_halt.gdb` loads the fixed Hello ELF,
halts again, and rejects a PC outside `0xF9000000..0xF9003FFF`.

Efinity's API proves the real Titanium outer-IR mapping and its `write_ir()`
selection behavior, but this static OpenOCD configuration has no traceable call
bridge to that API. The variable and tunnel settings alone are insufficient
evidence of real outer-IR selection. Therefore
`USER2_SELECTION_CHAIN=BLOCKED`; no USER2 session is authorized.

## Future Approved-Window Sequence

```text
CPU halted
  -> Hello ELF load
  -> halt and PC range gate
  -> Type-C UART1 PnP identity allowlist bind
  -> CAPTURE_READY
  -> resume once
  -> fixed Hello
  -> one printable ASCII byte only
  -> exact one-byte echo
  -> silent window and complete raw TX/RX evidence
```

UART1 is Type-C UART1 at `115200 8N1`. The capture tool rejects COM17, CH340,
unallowlisted PnP identity, control bytes, and automatic CR/LF behavior. It logs
`CAPTURE_READY`, a separate `RESUME_ONCE=<ISO-8601>` marker that cannot predate
capture readiness, every RX/TX byte timestamp and hexadecimal value, counts,
full byte transcript, and the silent-window end. The first failure ends the
session; no retry or fallback is permitted.

## WSC APB Consumption

Only WSC's contract at `48548f47dfa5964b13aed7edf3b3e9da6f6583a2` is consumed.
The CPU performs exactly one 32-bit read of `0xE8100000 + 0x000`; debugger APB
reads, APB writes, other offsets, scans, and retry are prohibited. After the
single resume, only a `BREAKPOINT` at `0xF90000C4` before `1000 ms` permits four
on-chip RAM evidence reads. Timeout, trap, wrong reason, or wrong PC records
the failure and leaves `RAM_READ_COUNT=0`.

The baseline has no `i0_uart1_cleanlf_apb_read.gdb`; no direct APB route has
been created. WSC's own Review Packet remains solely at its source SHA because
that Packet is absent from the `182fd6f` baseline.

## Exclusions

No `prepare_m2`, G1/R0/M2 operation card, UART0 checkpoint, COM approval JSON,
USER1, SoftTap, Flash, SPI, PROM, DDR, other TAP, cable scan, address scan, or
mechanical-arm route is part of this static contract.

`HARDWARE_ACTIONS=NONE`
