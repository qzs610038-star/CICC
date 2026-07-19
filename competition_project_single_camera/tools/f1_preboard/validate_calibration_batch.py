#!/usr/bin/env python3
"""Fail-closed validator for QW-CALIBRATION-SAMPLE-v1 batches.

This tool validates Git-safe batch identity files.  It never reads a camera,
opens a serial port, or claims a board result.  Pass --artifact-root to verify
raw-artifact bytes held outside the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path, PurePosixPath


SCHEMA = "QW-CALIBRATION-SAMPLE-v1"
REQUIRED_FILES = (
    "capture_profile.json",
    "sample_manifest.jsonl",
    "feature_rows.jsonl",
    "data_quality_summary.json",
    "sha256sums.json",
)
DATA_FILES = REQUIRED_FILES[:-1]
HEX64 = re.compile(r"^[0-9a-f]{64}$")
FEATURE_KEYS = {
    "schema", "batch_id", "sample_id", "object_id", "artifact_sha256",
    "evidence_level", "color", "shape", "nominal_size_cm_x10", "lighting_id",
    "position_id", "capture_profile_id", "frame_id", "config_seq", "source_flags",
    "red_area", "blue_area", "yellow_area", "foreground_area", "roi_pixel_count",
    "sum_luma", "bbox_width", "bbox_height", "expected_use",
}
MANIFEST_KEYS = {
    "schema", "batch_id", "sample_id", "object_id", "artifact_relpath",
    "artifact_bytes", "artifact_sha256", "color", "shape", "nominal_size_cm_x10",
    "lighting_id", "position_id", "capture_profile_id", "expected_use",
}
COLORS = {"WHITE", "BLACK", "RED", "BLUE", "YELLOW"}
SHAPES = {"CUBE", "CYLINDER", "CONE"}
USES = {"TRAIN", "CALIBRATE", "HOLDOUT", "NEGATIVE"}
MAX_COUNT = (1 << 21) - 1
MAX_LUMA = (1 << 31) - 1
MAX_BBOX = (1 << 11) - 1
REQUIRED_FLAGS = 0x47


class BatchError(Exception):
    pass


def fail(message: str) -> None:
    raise BatchError(message)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"{path.name}: invalid UTF-8 JSON: {exc}")


def load_jsonl(path: Path) -> list[dict]:
    rows = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as exc:
        fail(f"{path.name}: unreadable UTF-8 JSONL: {exc}")
    for number, line in enumerate(lines, 1):
        if not line.strip():
            fail(f"{path.name}:{number}: blank lines are forbidden")
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            fail(f"{path.name}:{number}: invalid JSON: {exc}")
        if not isinstance(value, dict):
            fail(f"{path.name}:{number}: expected object")
        rows.append(value)
    if not rows:
        fail(f"{path.name}: at least one row is required")
    return rows


def require_exact_keys(row: dict, keys: set[str], where: str) -> None:
    missing = sorted(keys - row.keys())
    extra = sorted(row.keys() - keys)
    if missing or extra:
        fail(f"{where}: missing={missing} extra={extra}")


def require_string(value, name: str) -> str:
    if not isinstance(value, str) or not value:
        fail(f"{name}: non-empty string required")
    return value


def require_int(value, name: str, low: int, high: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        fail(f"{name}: integer in [{low}, {high}] required")
    return value


def check_identity(row: dict, batch_id: str, where: str) -> None:
    if row.get("schema") != SCHEMA:
        fail(f"{where}: schema must be {SCHEMA}")
    if row.get("batch_id") != batch_id:
        fail(f"{where}: batch_id does not match capture profile")
    for field in ("sample_id", "object_id", "capture_profile_id"):
        require_string(row.get(field), f"{where}.{field}")
    artifact_hash = row.get("artifact_sha256")
    if not isinstance(artifact_hash, str) or not HEX64.fullmatch(artifact_hash):
        fail(f"{where}.artifact_sha256: lowercase SHA-256 required")
    if row.get("color") not in COLORS or row.get("shape") not in SHAPES:
        fail(f"{where}: unknown color or shape")
    if row.get("nominal_size_cm_x10") not in {20, 25, 30}:
        fail(f"{where}.nominal_size_cm_x10: expected 20, 25, or 30")
    if row.get("expected_use") not in USES:
        fail(f"{where}.expected_use: unknown split")
    for field in ("lighting_id", "position_id"):
        require_string(row.get(field), f"{where}.{field}")


def check_capture_profile(profile: dict) -> tuple[str, str]:
    keys = {
        "schema", "batch_id", "capture_profile_id", "camera_route", "frame_width",
        "frame_height", "camera_pose", "exposure", "white_balance", "lighting_id",
        "background", "roi", "thresholds",
    }
    require_exact_keys(profile, keys, "capture_profile")
    if profile["schema"] != SCHEMA:
        fail("capture_profile.schema mismatch")
    batch_id = require_string(profile["batch_id"], "capture_profile.batch_id")
    profile_id = require_string(profile["capture_profile_id"], "capture_profile.capture_profile_id")
    if profile["camera_route"] != "J48/ch0":
        fail("capture_profile.camera_route must be J48/ch0")
    width = require_int(profile["frame_width"], "capture_profile.frame_width", 1, 1920)
    height = require_int(profile["frame_height"], "capture_profile.frame_height", 1, 1080)
    for field in ("camera_pose", "exposure", "white_balance"):
        if not isinstance(profile[field], dict) or not profile[field]:
            fail(f"capture_profile.{field}: non-empty object required")
    if not isinstance(profile["lighting_id"], str) or not profile["lighting_id"]:
        fail("capture_profile.lighting_id: non-empty string required")
    background = profile["background"]
    if not isinstance(background, dict) or set(background) != {"r", "g", "b"}:
        fail("capture_profile.background: exact r/g/b object required")
    for channel in ("r", "g", "b"):
        require_int(background[channel], f"capture_profile.background.{channel}", 0, 255)
    roi = profile["roi"]
    if not isinstance(roi, dict) or set(roi) != {"x0", "y0", "x1", "y1"}:
        fail("capture_profile.roi: exact x0/y0/x1/y1 object required")
    x0 = require_int(roi["x0"], "capture_profile.roi.x0", 0, width - 1)
    x1 = require_int(roi["x1"], "capture_profile.roi.x1", 0, width - 1)
    y0 = require_int(roi["y0"], "capture_profile.roi.y0", 0, height - 1)
    y1 = require_int(roi["y1"], "capture_profile.roi.y1", 0, height - 1)
    if x0 > x1 or y0 > y1:
        fail("capture_profile.roi: closed ROI must satisfy x0<=x1 and y0<=y1")
    thresholds = profile["thresholds"]
    expected = {"foreground_delta", "red_min", "blue_min", "yellow_min", "color_delta"}
    if not isinstance(thresholds, dict) or set(thresholds) != expected:
        fail("capture_profile.thresholds: exact threshold object required")
    require_int(thresholds["foreground_delta"], "capture_profile.thresholds.foreground_delta", 0, 765)
    for field in expected - {"foreground_delta"}:
        require_int(thresholds[field], f"capture_profile.thresholds.{field}", 0, 255)
    return batch_id, profile_id


def check_manifest_rows(rows: list[dict], batch_id: str, profile_id: str, artifact_root: Path | None) -> dict[str, dict]:
    seen: dict[str, dict] = {}
    for index, row in enumerate(rows, 1):
        where = f"sample_manifest:{index}"
        require_exact_keys(row, MANIFEST_KEYS, where)
        check_identity(row, batch_id, where)
        if row["capture_profile_id"] != profile_id:
            fail(f"{where}: capture_profile_id mismatch")
        sample_id = row["sample_id"]
        if sample_id in seen:
            fail(f"{where}: duplicate sample_id {sample_id}")
        relative = row["artifact_relpath"]
        if not isinstance(relative, str) or not relative or os.path.isabs(relative):
            fail(f"{where}.artifact_relpath: relative path required")
        pure = PurePosixPath(relative.replace("\\", "/"))
        if pure.is_absolute() or ".." in pure.parts:
            fail(f"{where}.artifact_relpath: traversal is forbidden")
        byte_count = require_int(row["artifact_bytes"], f"{where}.artifact_bytes", 1, (1 << 63) - 1)
        if artifact_root is not None:
            candidate = artifact_root.joinpath(*pure.parts)
            if not candidate.is_file():
                fail(f"{where}: missing artifact {relative}")
            if candidate.stat().st_size != byte_count or sha256_file(candidate) != row["artifact_sha256"]:
                fail(f"{where}: artifact byte count or SHA-256 mismatch")
        seen[sample_id] = row
    return seen


def check_feature_rows(rows: list[dict], batch_id: str, profile_id: str, manifests: dict[str, dict]) -> None:
    split_by_object: dict[str, set[str]] = {}
    seen_sample_ids: set[str] = set()
    for index, row in enumerate(rows, 1):
        where = f"feature_rows:{index}"
        require_exact_keys(row, FEATURE_KEYS, where)
        check_identity(row, batch_id, where)
        if row["evidence_level"] != "HOST_CALIBRATION_PROVISIONAL":
            fail(f"{where}.evidence_level: board-preparation value required")
        if row["capture_profile_id"] != profile_id:
            fail(f"{where}: capture_profile_id mismatch")
        manifest = manifests.get(row["sample_id"])
        if manifest is None:
            fail(f"{where}: sample_id absent from manifest")
        if row["sample_id"] in seen_sample_ids:
            fail(f"{where}: duplicate sample_id {row['sample_id']}")
        seen_sample_ids.add(row["sample_id"])
        for field in ("object_id", "artifact_sha256", "color", "shape", "nominal_size_cm_x10", "lighting_id", "position_id", "capture_profile_id", "expected_use"):
            if row[field] != manifest[field]:
                fail(f"{where}.{field}: does not match manifest")
        require_int(row["frame_id"], f"{where}.frame_id", 0, 65535)
        require_int(row["config_seq"], f"{where}.config_seq", 0, 65535)
        if row["source_flags"] != REQUIRED_FLAGS:
            fail(f"{where}.source_flags: exactly stable/ROI/stats/ch0 (0x47) required")
        for field in ("red_area", "blue_area", "yellow_area", "foreground_area", "roi_pixel_count"):
            require_int(row[field], f"{where}.{field}", 0 if field != "roi_pixel_count" else 1, MAX_COUNT)
        require_int(row["sum_luma"], f"{where}.sum_luma", 0, MAX_LUMA)
        for field in ("bbox_width", "bbox_height"):
            require_int(row[field], f"{where}.{field}", 0, MAX_BBOX)
        if any(row[field] > row["roi_pixel_count"] for field in ("red_area", "blue_area", "yellow_area", "foreground_area")):
            fail(f"{where}: an area exceeds roi_pixel_count")
        if row["sum_luma"] > 255 * 3 * row["roi_pixel_count"]:
            fail(f"{where}: sum_luma exceeds per-pixel RGB bound")
        empty = row["foreground_area"] == 0
        if empty != (row["bbox_width"] == 0 and row["bbox_height"] == 0):
            fail(f"{where}: foreground/bbox empty invariant violated")
        split_by_object.setdefault(row["object_id"], set()).add(row["expected_use"])
    for object_id, uses in split_by_object.items():
        if "HOLDOUT" in uses and uses & {"TRAIN", "CALIBRATE"}:
            fail(f"object_id {object_id}: train/holdout leakage")


def check_quality_summary(summary: dict, batch_id: str, manifest_count: int, feature_rows: list[dict]) -> None:
    keys = {"schema", "batch_id", "evidence_level", "sample_count", "feature_row_count", "capture_status", "notes"}
    require_exact_keys(summary, keys, "data_quality_summary")
    if summary["schema"] != SCHEMA or summary["batch_id"] != batch_id:
        fail("data_quality_summary: schema or batch identity mismatch")
    if summary["evidence_level"] != "HOST_CALIBRATION_PROVISIONAL":
        fail("data_quality_summary.evidence_level mismatch")
    if summary["sample_count"] != manifest_count:
        fail("data_quality_summary.sample_count mismatch")
    if summary["feature_row_count"] != len(feature_rows):
        fail("data_quality_summary.feature_row_count mismatch")
    if summary["capture_status"] not in {"CAPTURED", "PARTIAL", "BLOCKED"}:
        fail("data_quality_summary.capture_status invalid")
    if not isinstance(summary["notes"], list):
        fail("data_quality_summary.notes must be an array")


def check_identity_manifest(batch: Path) -> None:
    manifest = load_json(batch / "sha256sums.json")
    if not isinstance(manifest, dict) or set(manifest) != {"schema", "batch_id", "files"}:
        fail("sha256sums.json: exact schema/batch_id/files object required")
    if manifest["schema"] != SCHEMA or not isinstance(manifest["batch_id"], str):
        fail("sha256sums.json: schema or batch_id invalid")
    files = manifest["files"]
    if not isinstance(files, dict) or set(files) != set(DATA_FILES):
        fail("sha256sums.json: four data file hashes required")
    for name in DATA_FILES:
        expected = files[name]
        if not isinstance(expected, str) or not HEX64.fullmatch(expected):
            fail(f"sha256sums.json: malformed hash for {name}")
        if sha256_file(batch / name) != expected:
            fail(f"sha256sums.json: stale hash for {name}")


def validate(batch: Path, artifact_root: Path | None, require_artifacts: bool) -> dict:
    if not batch.is_dir():
        fail("batch directory does not exist")
    for name in REQUIRED_FILES:
        if not (batch / name).is_file():
            fail(f"required file missing: {name}")
    if require_artifacts and artifact_root is None:
        fail("--require-artifacts requires --artifact-root")
    check_identity_manifest(batch)
    profile = load_json(batch / "capture_profile.json")
    batch_id, profile_id = check_capture_profile(profile)
    manifest_rows = load_jsonl(batch / "sample_manifest.jsonl")
    feature_rows = load_jsonl(batch / "feature_rows.jsonl")
    manifests = check_manifest_rows(manifest_rows, batch_id, profile_id, artifact_root)
    check_feature_rows(feature_rows, batch_id, profile_id, manifests)
    if len(manifests) != len(feature_rows):
        fail("every manifest sample must have exactly one feature row")
    check_quality_summary(load_json(batch / "data_quality_summary.json"), batch_id, len(manifests), feature_rows)
    return {"status": "PASS", "schema": SCHEMA, "batch_id": batch_id, "feature_rows": len(feature_rows), "artifact_bytes_verified": artifact_root is not None}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--batch", required=True, type=Path)
    parser.add_argument("--artifact-root", type=Path)
    parser.add_argument("--require-artifacts", action="store_true")
    args = parser.parse_args()
    try:
        result = validate(args.batch.resolve(), args.artifact_root.resolve() if args.artifact_root else None, args.require_artifacts)
    except BatchError as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
