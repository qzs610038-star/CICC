# CPU Hello UART1 H0–H6 Evidence Receipt and Integration Packet

## Scope

This packet is the only receipt gate for the next libaoxun fixed commit. It uses repository-relative paths, refs, complete Git SHA values, batch IDs, and SHA-256 values.

## Confirmed route; not a PASS claim

| Item | Current route | Status |
|---|---|---|
| I0 | SoC UART1 → Type-C UART1; `115200 8N1`; RX=`GPIOR_96/B12`; TX=`GPIOR_100/D12`; RAM=`0xF9000000..0xF9003FFF` | Team-confirmed route |
| H1 | `COM17` Type-C UART1 CH340; `COM10`/`COM13` J44/USER FTDI and prohibited | Team-confirmed identity pending fixed evidence |
| H3 | TAP=`0x006A0EF3`; BSCAN=`6,1`; inner IR=`9` | Team-confirmed route pending fixed evidence |
| H4/H5 | RAM load, PC gate, one resume, three-line Hello, one-byte echo | Waiting for fixed SHA and board evidence |

## H0–H6 receipt table

| Stage | Required receipt | Acceptance state |
|---|---|---|
| H0 static contract | fixed SHA; parent/base SHA; actual diff; atomic-input SHA-256; same-batch `soc.h` summary | `WAITING_LIBAOXUN_FIXED_SHA` |
| H1 port identity | complete `COM17` PnP tuple, device identity, physical topology statement, time | `WAITING_LIBAOXUN_FIXED_SHA` |
| H2 volatile configuration | bitstream SHA-256, tool identity, JTAG ID, time, command/result, no Flash/DDR/USER1 declaration | `WAITING_LIBAOXUN_FIXED_SHA` |
| H3 USER2/DTM | raw DTM/Target-examined markers for TAP/BSCAN/inner-IR route | `WAITING_LIBAOXUN_FIXED_SHA` |
| H4 RAM/Hello TX | ELF SHA-256; PC gate; one resume; raw three-line UART1 bytes; exit/status | `WAITING_LIBAOXUN_FIXED_SHA` |
| H5 RX echo | one approved printable byte sent once and exactly one identical byte echoed | `WAITING_LIBAOXUN_FIXED_SHA` |
| H6 APB handoff | not run, or an explicit reason and separate APB review reference | `NOT_VERIFIED` |

## Fixed-commit intake checklist

- Remote ref, complete commit SHA, parent SHA, baseline SHA, actual changed-file list.
- Bitstream, Hello ELF and `soc.h` SHA-256; Efinity/OpenOCD/GDB/execution-tool identity; commands, timestamps, exit codes and warnings.
- H0–H6 evidence above; preserved indices for all failed windows; remaining `NOT VERIFIED` items.
- Declaration of no Flash, DDR, USER1, UART0, `COM10`, `COM13`, APB, UART2/J52 or myCobot scope breach.
- Git payload limited to source, reproducible scripts, manifest/verifier, sanitized evidence summary, raw-evidence index and SHA-256. No binary payloads, outflow/work trees, temporary databases, private board configuration or unsanitized raw logs.

## Supersession matrix

| Historical conclusion | Replacement |
|---|---|
| UART0/G1/R0 is next CPU Hello route | `HISTORICAL / SUPERSEDED`; current route is I0 UART1 |
| old COM mapping selects a current port | `HISTORICAL / SUPERSEDED`; current H1 is `COM17` only |
| inner IR=`8` or CPU TAP=`0x006A0A79` defines H3 | `HISTORICAL / SUPERSEDED`; current H3 is TAP `0x006A0EF3`, BSCAN `6,1`, inner IR=`9` |
| Host P1 PASS proves board CPU Hello | False; Host evidence remains Host-only |
| USER2, CPU, UART1 and APB are one status | False; H3/H4/H5/APB are independent gates |

## Post-merge freshness checklist

1. Verify fixed SHA, parent, scope and atomic-input status.
2. Run P1, adapter, classifier, F1, runtime/G2, manifest/tamper and offline presubmit from the new merge HEAD.
3. Run freeze, freshness, handoff and `git diff --check`; record fresh counts and exit codes only.
4. Refresh `CURRENT_STATE.md` and `SESSION_HANDOFF.md`; do not update `MERGE_REGISTER.md` before an actual `main` merge.
