# QZS Goal4 I0 UART1 fixed-SHA final static acceptance

> Acceptance time: 2026-07-19 (Asia/Shanghai)
>
> Scope: static fixed-SHA acceptance only. This record neither opens nor consumes a hardware window.

## Fixed identities

| Role | Fixed identity | Static result |
|---|---|---|
| QZS governance base | `f027c6ff7b087e4dff5c23b5d76edd8286172f1f` | Context only; no history merge or migration |
| libaoxun execution checkout | `b4771979e7154ecbee2d3ec45e16822b1199112a` | PASS; detached clean checkout HEAD matched |
| libaoxun code parent | `11d4580b49e4e280c29e608179196b17d687c723` | PASS; immediate parent |
| patch-chain base | `9949e6ed737f25db82111cc38250dfc15bdb54c9` | PASS; verifier scope baseline |
| WSC contract checkout | `48548f47dfa5964b13aed7edf3b3e9da6f6583a2` | PASS; detached clean checkout HEAD matched |
| execution manifest | SHA-256 `8C9E4ADA9BF14FB7B827B34FFB2FE13B93D84AE7037A21DEE1FF2AB6A932FDC2` | PASS; recomputed |

Both commits were read from `origin` by `git ls-remote --heads`, fetched by full SHA, and checked out in separate detached worktrees. Both had empty `git status --porcelain --untracked-files=all` output.

## Static verifier evidence

| Check | Result |
|---|---|
| WSC contract | `verify_apb_probe_contract.ps1` exit 0; success and four negative models PASS; `HARDWARE_ACTIONS=NONE` |
| libaoxun execution configuration | `verify_i0_uart1_execution_config.ps1` exit 0; manifest/runtime binding, dirty/wrong-head/hash/tool negatives, BSCAN `6 1` / USER2 `0x09`, exact UART1 PnP allowlist, WSC binding, EOL and mock fixtures PASS; `HARDWARE_ACTIONS=NONE` |
| range and whitespace | `git diff --check` for `11d4580b..b4771979` and `182fd6f..48548f47`: PASS |
| EOL | verifier PASS: runtime PS1=CRLF; CFG/GDB=LF; manifest matches |
| dangerous routes | `TEMP_SCOPE_DANGER_ROUTE=PASS`; COM17, CH340, J44/UART0/programmer, Flash, DDR, UART2/J52, retry/fallback, direct APB and mechanical-arm routes are prohibited |

The manifest binds OpenOCD SHA-256 `FD8F9242B8BA34C33A22409103F0DFA9798070BEC64F3549F11CB911B312151A` and GDB SHA-256 `BF89D58E62CDFAF4729518FA19AC6626B3219F3FF26F5A981593723F5EF3DFCB`. This acceptance verified the declared bindings and fail-closed path/hash/version checks; it did not access installed tools or launch OpenOCD, GDB, Efinity, serial, JTAG, or another hardware-facing process.

## Approval status and required dynamic fields

`I0_UART1_EXTERNAL_APPROVAL_SCHEMA_V2_TEMPLATE_20260719.json` is unsigned and non-executable. Do not issue it until the new window request supplies and independently checks:

1. `board_id`.
2. Exact Type-C UART1 `VID/PID/serial/instance` and matching COM number.
3. UTC `window_start_utc` and `window_end_utc`.

`COM17`, `CH340`, and every `J44`/`UART0` programmer identity are prohibited. `approved_commit` is fixed to `b4771979e7154ecbee2d3ec45e16822b1199112a`; never substitute a parent, base, WSC SHA, or branch name.

## Final acceptance

```text
QZS_FINAL_STATIC_ACCEPTANCE=PASS
EXECUTION_COMMIT=b4771979e7154ecbee2d3ec45e16822b1199112a
VERDICT=READY_FOR_NEW_WINDOW_REQUEST
HARDWARE_ACTIONS=NONE
```

This is readiness to request a separately approved window only. It is not USER2, UART1 Hello/echo, APB MAGIC, board, UART2/J52, or myCobot evidence. P0/P1 Host work remains independent and may continue under its existing scope.
