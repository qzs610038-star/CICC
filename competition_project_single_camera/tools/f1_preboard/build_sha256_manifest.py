#!/usr/bin/env python3
"""Create or verify the four-file QW-CALIBRATION-SAMPLE-v1 identity manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


DATA_FILES = ("capture_profile.json", "sample_manifest.jsonl", "feature_rows.jsonl", "data_quality_summary.json")
SCHEMA = "QW-CALIBRATION-SAMPLE-v1"


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--batch", required=True, type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    batch = args.batch.resolve()
    try:
        profile = json.loads((batch / "capture_profile.json").read_text(encoding="utf-8"))
        if profile.get("schema") != SCHEMA or not isinstance(profile.get("batch_id"), str) or not profile["batch_id"]:
            raise ValueError("capture_profile identity invalid")
        files = {name: digest(batch / name) for name in DATA_FILES}
        payload = {"schema": SCHEMA, "batch_id": profile["batch_id"], "files": files}
        target = batch / "sha256sums.json"
        if args.verify:
            current = json.loads(target.read_text(encoding="utf-8"))
            if current != payload:
                raise ValueError("sha256sums.json does not match current four data files")
            print(json.dumps({"status": "PASS", "batch_id": profile["batch_id"], "files": len(files)}, sort_keys=True))
        else:
            target.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps({"status": "WROTE", "batch_id": profile["batch_id"], "files": len(files)}, sort_keys=True))
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)}))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
