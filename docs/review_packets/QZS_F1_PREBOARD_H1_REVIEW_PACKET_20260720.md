# QZS F1 Preboard B0/H1 Review Packet

> Status: `PREBOARD_INPUT_ONLY / NOT BOARD APPROVAL`
>
> Seed: `40b42ddcbd5f9bb52ea0482203aa78e0d446aaad`
>
> Invariants: `BOARD_VERIFIED=NO`, `P0_B=HOLD`, `ARM_ENABLED=0`

## 任务目标与当前结论

This packet prepares only two fail-closed inputs:

1. B0 capture verification of exactly one `QW-F1-BOARD-SELFTEST-v1` summary after a future independently approved UART1 chain; and
2. the H1 five-level evidence ladder after B0 succeeds.

It does not authorize Efinity, USER2, UART, a board connection, APB guessing, a frozen-interface change, or any myCobot activity. `verify_f1_board_selftest.py` accepts exactly one ASCII/LF line with the fixed ordered fields `build`, `cases`, `pass`, `digest`, and `arm=0`. A duplicate, malformed/garbled line, wrong digest, unequal counts, missing field, or additional field is a failure.

## 修改文件与关键 diff

- `competition_project_single_camera/tools/f1_preboard/verify_f1_board_selftest.py`: offline capture-file verifier only; it never opens a serial device.
- `competition_project_single_camera/tools/f1_preboard/tests/test_f1_preboard_tools.py`: positive and duplicate-summary negative case.
- No CPU, RTL, top, XML, SDC, IP, generated BSP, `soc.h`, `embedded_sw`, or frozen `integration/**` files changed.

## 模块、信号、时钟、复位和双通道影响

No hardware signal, clock, reset, CDC, frame-buffer, APB, UART, OSD, or video path changes are present. The following is an evidence sequence, not a wire ABI:

| Level | Required future evidence | Stop condition |
|---|---|---|
| B0 tap | Approved fixed hardware SHA/batch and actual `USER2 -> RAM Hello -> UART1 three lines/echo -> APB` evidence | Any identity/hash/chain failure; do not load F1 ELF |
| H1.1 snapshot/ACK/CDC | libaoxun-owned atomic RTL/SoC batch; single-slot, same-frame ACK, overrun and multi-bit CDC evidence | No frozen-interface user token or any CDC/atomicity failure |
| H1.2 CPU snapshot | wsc backend consumes only the generated, reviewed ABI and proves stable/re-read/ACK rules | Missing ABI or torn/invalid snapshot |
| H1.3 result/OSD | Separate staging/commit/round-id and OSD evidence | No result/OSD ABI or video regression |
| H1.4 input | Approved input/event evidence after prior levels | Input semantics incomplete or any external-control expansion |

`ARM_ENABLED=0` remains required at every level. UART2/J52 and myCobot are outside this packet.

## 已运行命令、退出码、日志与 warning

The verifier and its local positive/negative fixtures are run by the Q0-Q5 execution record. No Efinity, USER2, UART, board, warning, Map/PNR/STA, or serial capture command has been run by this packet.

## 未验证项、风险假设与回退条件

- `QW-F1-BOARD-SELFTEST-v1` build ID/digest/case count: awaiting a future wsc fixed source/ELF batch.
- UART1, USER2, RAM Hello/echo, APB, CPU execution, feature snapshot, ACK/CDC, result/OSD, and input: `NOT VERIFIED`.
- Q2 real camera data and a real calibration batch: `BLOCKED_NO_FIXED_CAPTURE_INPUT` in this worktree.
- If any identity check fails, retain the prior hardware batch as evidence only and create no claim of B0 or H1 progress.

## 机械臂/外设状态与用户安全确认

`ARM_ENABLED=0`. No UART2/J52 connection, query, frame, or motion is authorized or performed. This packet contains no live peripheral command.

## 希望 Codex 裁定的问题

Before B0: confirm that libaoxun has voluntarily supplied one fixed hardware batch whose USER2, RAM Hello/echo, and APB chain actually passed, and that a separate user approval covers the new F1 ELF. Before H1: obtain the complete frozen-interface modification token and a hardware Review Packet owned by libaoxun.
