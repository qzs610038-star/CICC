# F1 Preboard calibration batches

`QW-CALIBRATION-SAMPLE-v1` is a qzs-to-wsc **Host/board-preparation** exchange format. It is not an FPGA wire ABI, APB map, CDC design, or board approval.

Each published batch contains exactly these five identity files:

```text
<batch_id>/
  capture_profile.json
  sample_manifest.jsonl
  feature_rows.jsonl
  data_quality_summary.json
  sha256sums.json
```

Raw frames and video stay outside Git. `sample_manifest.jsonl` records only a logical relative artifact name, byte count, and SHA-256. Run the validator with `--artifact-root` when the raw-artifact store is available; this is required for a release-grade validation.

The capture profile is fixed to the single-camera J48/ch0 view and records camera pose, optics, exposure, white balance, lighting, background, and inclusive ROI. A changed profile, raw artifact, or label creates a new batch; never rewrite a published batch in place.

## Commands

```powershell
python competition_project_single_camera/tools/f1_preboard/build_sha256_manifest.py --batch <batch-dir>
python competition_project_single_camera/tools/f1_preboard/validate_calibration_batch.py --batch <batch-dir> --artifact-root <raw-root> --require-artifacts
python competition_project_single_camera/tools/f1_preboard/build_sha256_manifest.py --batch <batch-dir> --verify
```

The validator rejects unknown or missing feature fields, non-J48/ch0 profiles, absolute artifact paths, duplicate `sample_id`, cross-split `object_id`, invalid flags, stale hashes, counter/ROI violations, and non-zero bboxes with no foreground. Board-preparation output remains `HOST_CALIBRATION_PROVISIONAL`; it never upgrades feature/APB/OSD or board status.

## Current data status

No real capture batch is checked in by this change. Real J48/ch0 capture needs a fixed camera, object/sample set, and recorded illumination/ROI conditions. Until those are available, Q2 is `BLOCKED_NO_FIXED_CAPTURE_INPUT`; synthetic unit fixtures only exercise the format and RTL-equivalent Host arithmetic.
