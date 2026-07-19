from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "extract_golden_features.py"
VALIDATE = ROOT / "validate_calibration_batch.py"
BUILD = ROOT / "build_sha256_manifest.py"
SELFTEST = ROOT / "verify_f1_board_selftest.py"


def run(*args: str, expect: int = 0) -> subprocess.CompletedProcess:
    result = subprocess.run([sys.executable, *args], text=True, capture_output=True, check=False)
    if result.returncode != expect:
        raise AssertionError(f"expected {expect}, got {result.returncode}\nstdout={result.stdout}\nstderr={result.stderr}")
    return result


class F1PreboardToolTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.batch = self.root / "batch"
        self.batch.mkdir()
        self.artifacts = self.root / "raw"
        frame = self.artifacts / "frames" / "sample.png"
        frame.parent.mkdir(parents=True)
        image = Image.new("RGB", (4, 2), (0, 0, 0))
        image.putpixel((0, 0), (220, 10, 10))
        image.putpixel((1, 0), (10, 20, 230))
        image.putpixel((2, 0), (220, 220, 10))
        image.save(frame)
        self.frame = frame
        self.profile = {
            "schema": "QW-CALIBRATION-SAMPLE-v1", "batch_id": "qzs-fixture-v1",
            "capture_profile_id": "j48-ch0-fixture", "camera_route": "J48/ch0",
            "frame_width": 4, "frame_height": 2,
            "camera_pose": {"height_mm": 100, "pitch_deg": 5, "focal_length_mm": 4},
            "exposure": {"mode": "MANUAL", "value": "10ms"},
            "white_balance": {"mode": "MANUAL", "value": "5000K"}, "lighting_id": "fixture-light",
            "background": {"r": 0, "g": 0, "b": 0}, "roi": {"x0": 0, "y0": 0, "x1": 3, "y1": 1},
            "thresholds": {"foreground_delta": 20, "red_min": 180, "blue_min": 180, "yellow_min": 180, "color_delta": 20},
        }
        self.metadata = {
            "batch_id": "qzs-fixture-v1", "sample_id": "sample-001", "object_id": "cube-red-01",
            "color": "RED", "shape": "CUBE", "nominal_size_cm_x10": 20,
            "lighting_id": "fixture-light", "position_id": "center", "frame_id": 1, "config_seq": 9,
            "expected_use": "CALIBRATE",
        }
        (self.batch / "capture_profile.json").write_text(json.dumps(self.profile), encoding="utf-8")
        metadata_path = self.root / "metadata.json"
        metadata_path.write_text(json.dumps(self.metadata), encoding="utf-8")
        output = self.batch / "feature.json"
        run(str(EXTRACT), "--image", str(frame), "--capture-profile", str(self.batch / "capture_profile.json"), "--metadata", str(metadata_path), "--output", str(output))
        feature = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual((feature["red_area"], feature["blue_area"], feature["yellow_area"]), (1, 1, 1))
        self.assertEqual((feature["foreground_area"], feature["bbox_width"], feature["bbox_height"]), (3, 3, 1))
        digest = hashlib.sha256(frame.read_bytes()).hexdigest()
        manifest = {**{key: self.metadata[key] for key in ("batch_id", "sample_id", "object_id", "color", "shape", "nominal_size_cm_x10", "lighting_id", "position_id", "expected_use")}, "schema": "QW-CALIBRATION-SAMPLE-v1", "capture_profile_id": "j48-ch0-fixture", "artifact_relpath": "frames/sample.png", "artifact_bytes": frame.stat().st_size, "artifact_sha256": digest}
        (self.batch / "sample_manifest.jsonl").write_text(json.dumps(manifest) + "\n", encoding="utf-8")
        (self.batch / "feature_rows.jsonl").write_text(json.dumps(feature) + "\n", encoding="utf-8")
        quality = {"schema": "QW-CALIBRATION-SAMPLE-v1", "batch_id": "qzs-fixture-v1", "evidence_level": "HOST_CALIBRATION_PROVISIONAL", "sample_count": 1, "feature_row_count": 1, "capture_status": "CAPTURED", "notes": ["synthetic fixture only"]}
        (self.batch / "data_quality_summary.json").write_text(json.dumps(quality), encoding="utf-8")
        run(str(BUILD), "--batch", str(self.batch))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def validate(self, batch: Path | None = None, expect: int = 0) -> subprocess.CompletedProcess:
        return run(str(VALIDATE), "--batch", str(batch or self.batch), "--artifact-root", str(self.artifacts), "--require-artifacts", expect=expect)

    def copy_batch(self) -> Path:
        target = self.root / f"copy-{len(list(self.root.glob('copy-*')))}"
        shutil.copytree(self.batch, target)
        return target

    def refresh_hashes(self, batch: Path) -> None:
        run(str(BUILD), "--batch", str(batch))

    def test_valid_batch_and_manifest_verify(self) -> None:
        self.validate()
        run(str(BUILD), "--batch", str(self.batch), "--verify")

    def test_tampered_artifact_hash_fails(self) -> None:
        batch = self.copy_batch()
        row = json.loads((batch / "feature_rows.jsonl").read_text(encoding="utf-8"))
        row["artifact_sha256"] = "0" * 64
        (batch / "feature_rows.jsonl").write_text(json.dumps(row) + "\n", encoding="utf-8")
        self.refresh_hashes(batch)
        self.validate(batch, expect=1)

    def test_duplicate_sample_id_fails(self) -> None:
        batch = self.copy_batch()
        manifest = json.loads((batch / "sample_manifest.jsonl").read_text(encoding="utf-8"))
        (batch / "sample_manifest.jsonl").write_text((batch / "sample_manifest.jsonl").read_text(encoding="utf-8") + json.dumps(manifest) + "\n", encoding="utf-8")
        row = json.loads((batch / "feature_rows.jsonl").read_text(encoding="utf-8"))
        (batch / "feature_rows.jsonl").write_text(json.dumps(row) + "\n" + json.dumps(row) + "\n", encoding="utf-8")
        quality = json.loads((batch / "data_quality_summary.json").read_text(encoding="utf-8"))
        quality["sample_count"] = quality["feature_row_count"] = 2
        (batch / "data_quality_summary.json").write_text(json.dumps(quality), encoding="utf-8")
        self.refresh_hashes(batch)
        self.validate(batch, expect=1)

    def test_object_train_holdout_leakage_fails(self) -> None:
        batch = self.copy_batch()
        manifest = json.loads((batch / "sample_manifest.jsonl").read_text(encoding="utf-8"))
        manifest["sample_id"] = "sample-002"
        manifest["expected_use"] = "HOLDOUT"
        (batch / "sample_manifest.jsonl").write_text((batch / "sample_manifest.jsonl").read_text(encoding="utf-8") + json.dumps(manifest) + "\n", encoding="utf-8")
        feature = json.loads((batch / "feature_rows.jsonl").read_text(encoding="utf-8"))
        feature["sample_id"] = "sample-002"
        feature["expected_use"] = "HOLDOUT"
        (batch / "feature_rows.jsonl").write_text((batch / "feature_rows.jsonl").read_text(encoding="utf-8") + json.dumps(feature) + "\n", encoding="utf-8")
        quality = json.loads((batch / "data_quality_summary.json").read_text(encoding="utf-8"))
        quality["sample_count"] = quality["feature_row_count"] = 2
        (batch / "data_quality_summary.json").write_text(json.dumps(quality), encoding="utf-8")
        self.refresh_hashes(batch)
        self.validate(batch, expect=1)

    def test_selftest_capture_is_strict(self) -> None:
        capture = self.root / "capture.txt"
        capture.write_text("@F1SELFTEST|v=1|build=f1-abc|cases=3|pass=3|digest=deadbeef|arm=0\n", encoding="ascii", newline="\n")
        run(str(SELFTEST), "--capture", str(capture), "--expected-build", "f1-abc", "--expected-cases", "3", "--expected-digest", "deadbeef")
        capture.write_text(capture.read_text(encoding="ascii") + "@F1SELFTEST|v=1|build=f1-abc|cases=3|pass=3|digest=deadbeef|arm=0\n", encoding="ascii", newline="\n")
        run(str(SELFTEST), "--capture", str(capture), "--expected-build", "f1-abc", "--expected-cases", "3", "--expected-digest", "deadbeef", expect=1)


if __name__ == "__main__":
    unittest.main()
