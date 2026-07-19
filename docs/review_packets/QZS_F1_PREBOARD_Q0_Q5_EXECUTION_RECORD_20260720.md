# QZS F1 Preboard Q0-Q5 execution record

> Fixed published seed: `40b42ddcbd5f9bb52ea0482203aa78e0d446aaad`
>
> Personal branch: `codex/qzs-f1-preboard-data-evidence-20260720`
>
> Status ceiling: `QZS_PREBOARD_INPUTS_READY` only after the listed offline gates pass.

## Q0 — completed safe start

- Created `D:\CICC-qzs-f1-preboard-data-evidence-20260720` from the exact published seed; no reset, clean, stash, fetch, merge, or libaoxun worktree/ref operation was used.
- Handoff health check, initial qzs scope check, interface-freeze check, and `git diff --check` passed on a clean personal branch.

## Q1 — schema, identity manifest, and validator

- Added strict `QW-CALIBRATION-SAMPLE-v1` schema, capture/sample templates, manifest writer, and validator under qzs-owned docs/tools paths.
- The validator requires exactly the four data-file hashes, canonical J48/ch0 capture profile, relative raw-artifact references, exact feature fields, `0x47` source flags, counter/bbox invariants, and object-level train/holdout isolation.

## Q2 — real capture status

`BLOCKED_NO_FIXED_CAPTURE_INPUT`. No user-confirmed fixed J48/ch0 camera pose, real object/sample set, lighting/background profile, or authorized raw-artifact location was available in this task. No camera enumeration/capture was attempted and no synthetic fixture is represented as real data. This does not block Q1, Q3/Q4 tooling, or Q5 packet preparation.

## Q3/Q4 — Host golden features and publication gate

- Added a Pillow-backed extractor which processes RGB images as contract-order p0/p1 48-bit pairs, applies closed ROI, three RTL-equivalent masks, RGB background-delta foreground, `R+G+B` luma, inclusive bbox, and frozen 21/31-bit bounds.
- Every output is `HOST_CALIBRATION_PROVISIONAL`; it cannot certify FPGA/APB/CDC/OSD/board behavior.
- No real QW batch is published. A real batch requires an external raw-artifact root and `--require-artifacts`, then a regenerated four-file SHA-256 manifest. Any correction creates a new `batch_id`.

## Q5 — B0 verifier and H1 packet

- Added an offline-only capture-file verifier and the H1 packet at `docs/review_packets/QZS_F1_PREBOARD_H1_REVIEW_PACKET_20260720.md`.
- B0 is limited to a future independently approved UART1 chain and one matching F1 selftest summary. Feature/APB/OSD/input remain `NOT VERIFIED`.

## Persistent safety state

```text
BOARD_VERIFIED=NO
P0_B=HOLD
ARM_ENABLED=0
UART2_J52=OUT_OF_SCOPE
```

## Offline validation transcript

All commands below ran in `D:\CICC-qzs-f1-preboard-data-evidence-20260720` and returned exit code `0` unless a unit test intentionally expected a validator/verifier failure.

```text
tools/agent_handoff_health_check.ps1
tools/team_scope_check.ps1 -Role qzs -BaseRef 40b42ddcbd5f9bb52ea0482203aa78e0d446aaad -TargetRef HEAD  (clean Q0 seed)
tools/interface_freeze_check.ps1
git diff --check 40b42ddcbd5f9bb52ea0482203aa78e0d446aaad
python -m unittest discover -s competition_project_single_camera/tools/f1_preboard/tests -v
```

The unit suite passed `5/5`: valid batch/manifest, tampered artifact hash, duplicated `sample_id`, object train/holdout leakage, and duplicate B0 summary. The three failing calibration cases use only system-temporary raw fixtures and fail non-zero by design; no raw image, database, absolute capture path, serial capture, or board artifact is committed.
