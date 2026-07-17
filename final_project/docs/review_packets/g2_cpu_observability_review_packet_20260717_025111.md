# G2 Review Packet — CPU runtime seam and offline observability L0

Date: 2026-07-17 02:51:11 (Asia/Shanghai)
Baseline: `489ab5b0c1773bfcb776cedd9f39b3f088fb4a0f`
Worktree: `D:\CICC-g2-cpu-observability`
Scope: Host/fake transport only; no FPGA, XML, SDC, APB address, UART, COM, or mechanical-arm access.

## Changes

- CPU runtime seam: `competition_project_single_camera/cpu/include/single_camera_runtime.h` and `src/single_camera_runtime.c`.
- Deterministic fake transport and explicit fail-closed MMIO placeholder: `include/` and `src/` `single_camera_{fake,mmio}_transport.*`.
- MSVC-discovering one-shot Host runner and 182-assertion runtime test: `cpu/tests/run_g2_host_evidence.ps1` and `test_single_camera_runtime.c`.
- Offline bundle generator, validator, seven negative raw-log fixtures, and Python regression: `final_project/tools/board_observability/`.

## Minimal call chain

```text
fake transport read_feature_snapshot
  -> snapshot validation
  -> single_camera_feature_adapter
  -> single_camera_classifier
  -> sc_f1_observe
  -> decision/ROUND_RESULT
  -> fake transport ack_feature_frame + submit_round_result
  -> @E structured event
  -> raw.log/events.jsonl/manifest.json/summary.md
```

`sc_runtime_transport_t` has exactly four platform-independent operations: `read_feature_snapshot`, `ack_feature_frame`, `submit_round_result`, and `emit_diagnostic_event`. The fake implementation is the only working backend. `sc_mmio_transport_init_fail_closed()` deliberately provides no address or protocol and every operation returns `SC_TRANSPORT_UNAVAILABLE`.

## Event schema and stable reasons

All records have deterministic fields: `v`, `seq`, `event`, `round`, `frame`, `cfg`, `flags`, `class`, `decision`, `reason`, `ack`, `arm`, and `source`.

| Category | Stable values exercised |
|---|---|
| Lifecycle | `BOOT`, `SNAPSHOT_ACCEPT`, `CLASSIFY`, `DECISION`, `ACK`, `ROUND_RESULT` |
| Suppression/recovery | `DUPLICATE_SUPPRESSED`, `TIMEOUT`, `ABANDON`, `FATAL` |
| Fail-closed reason | `SNAPSHOT_FLAGS_REJECTED`, `SNAPSHOT_TORN`, `CONFIG_REVISION_MISMATCH`, `ACK_MISMATCH`, `DUPLICATE_FRAME`, `PLACE_DUPLICATE`, `CLASSIFICATION_UNSTABLE` |
| F1 semantic reason | `TARGET_MATCH_ARM_DISABLED`, `COLOR_MISMATCH_SKIP`, `SHAPE_MISMATCH_SKIP`, `SIZE_UNAVAILABLE`, `ROUND_TIMEOUT`, `OPERATOR_ABANDONED` |

## Test matrix and results

- Normal target and non-target decisions; five colors (white/black/red/blue/yellow); cube and non-cube.
- Invalid flags, overflow, overrun, torn snapshot, config mismatch, ACK mismatch, duplicate frame, duplicate PLACE, timeout, abandon, size unavailable, and unclassifiable features.
- 20 deterministic Host transactions (four groups of five), with 20 ACKs and 20 submitted results; it is a transaction stress vector, not an assertion that all four competition tasks are complete.
- Offline negative fixtures: mixed hash, missing BOOT, missing ACK, ACK mismatch, malformed event, sequence error, duplicate result, and non-zero ARM state.

Executed command:

```powershell
& 'D:\CICC-g2-cpu-observability\competition_project_single_camera\cpu\tests\run_g2_host_evidence.ps1' -RunDir 'D:\CICC-g2-run-bundle-20260717-r5'
```

Result: exit `0`; MSVC `19.42.34436`; runtime assertions `182/182`; bundle validation passed. Python negative-fixture regression: `3/3` tests passed.

## Evidence bundle

`D:\CICC-g2-run-bundle-20260717-r5`

- `manifest.json`: baseline HEAD, dirty state, compiler, full command, input SHA-256, counts, and explicit boundaries.
- `raw.log`: 101 raw events plus test summary; original malformed input is retained by the tool rather than rewritten.
- `events.jsonl`: deterministic parsed records.
- `summary.md`: 182/182, 101 events, 20 ACKs, and ARM request/send `0/0`.

## Non-verified boundaries and APB next step

- **HOST VERIFIED**, **FAKE TRANSPORT VERIFIED**, and **OFFLINE OBSERVABILITY L0 VERIFIED** only.
- **RISC_V_ELF_NOT_BUILT**, **FPGA_APB_NOT_IMPLEMENTED**, **BOARD NOT VERIFIED**, and **ARM DISABLED**.
- No COM port was opened and no serial byte, UART byte, myCobot frame, or mechanical action was sent. The only file I/O in the fake path is Host `raw.log` creation.
- A future real backend must first receive an independently reviewed feature-register ABI: atomic read/commit semantics, frame/config revisions, ACK behavior, clock/reset ownership, and validated APB address mapping. It must then replace only the fail-closed transport implementation and preserve the same event schema/tests.

## Suggested `CURRENT_STATE.md` append (not applied)

> G2 completed an offline-only CPU runtime seam with a deterministic fake transport, fail-closed MMIO placeholder, structured `@E|v=1` events, 20 Host/fake transactions, and a self-validating evidence bundle. Host/fake transport and Offline Observability L0 are verified; RISC-V ELF, FPGA APB, board UART, and ARM remain not verified/disabled. No APB address or serial protocol was inferred.

## Codex final-integration retest — 2026-07-17 12:58

- Fresh rerun first exposed a Windows PowerShell native-argument quoting failure when the full compiler command containing `Program Files` was passed directly to Python `argparse`; C runtime assertions had passed, but the runner correctly returned exit 2 and was not accepted.
- The runner now transports only that manifest field as UTF-8 Base64 and the Python tool decodes it before recording the unchanged command text. No runtime, event, MMIO, UART or hardware behavior changed. Direct-call tests retain compatibility via `getattr`.
- Fresh runner: `D:\CICC-g2-run-bundle-finalpass-20260717_125840`, `182/182`, `VALIDATION_PASS`, exit 0.
- Python negative/valid bundle regression: 3 tests, exit 0.
- `git diff --check`: exit 0.
