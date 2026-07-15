#!/usr/bin/env python3
"""Fail-closed verifier for a myCobot G4 SoC/PNR deployment evidence batch.

This tool validates evidence only.  It never opens Efinity, invokes a
Programmer, accesses a serial port, or creates an FPGA/CPU build artifact.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA = "mycobot-g4-batch-v1"
REQUIRED_ASSETS = (
    "project_xml",
    "periphery_xml",
    "sdc",
    "top",
    "soc_h",
    "linker",
    "startup",
    "elf",
    "bitstream",
    "map_log",
    "pnr_log",
    "sta_log",
)
PLACEHOLDER_TOKENS = (
    "STANDALONE_TEST",
    "0xF0000000",
    "APB_VISION_BASE_PLACEHOLDER",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def resolve_path(manifest_path: Path, value: str) -> Path:
    candidate = Path(value)
    return candidate if candidate.is_absolute() else manifest_path.parent / candidate


def require_string(value: Any, label: str, errors: list[str]) -> str:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"missing/non-string {label}")
        return ""
    return value


def check_text_contains(path: Path, assertions: Any, label: str, errors: list[str]) -> None:
    if not isinstance(assertions, list) or not assertions or not all(
        isinstance(item, str) and item for item in assertions
    ):
        errors.append(f"{label} assertions must be a non-empty string list")
        return
    content = path.read_text(encoding="utf-8", errors="replace")
    for item in assertions:
        if item not in content:
            errors.append(f"{label} assertion not found in {path.name}: {item!r}")


def verify(manifest_path: Path) -> dict[str, Any]:
    errors: list[str] = []
    observed: dict[str, str] = {}
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return {"result": "FAIL", "errors": [f"cannot read manifest: {exc}"], "observed_sha256": {}}

    if manifest.get("schema") != SCHEMA:
        errors.append(f"schema must be {SCHEMA!r}")
    build_id = require_string(manifest.get("build_id"), "build_id", errors)
    commit = require_string(manifest.get("commit"), "commit", errors)
    if commit and not re.fullmatch(r"[0-9a-fA-F]{7,64}", commit):
        errors.append("commit must be a 7-64 digit hexadecimal revision")
    require_string(manifest.get("efinity_version"), "efinity_version", errors)
    if manifest.get("profile") != "arm_bringup":
        errors.append("profile must be 'arm_bringup' for the G4 CPU-Hello proof")
    if manifest.get("backend") not in ("disabled", "simulated"):
        errors.append("backend must be 'disabled' or 'simulated'")

    assets = manifest.get("assets")
    hashes = manifest.get("sha256")
    paths: dict[str, Path] = {}
    if not isinstance(assets, dict):
        errors.append("assets must be an object")
    if not isinstance(hashes, dict):
        errors.append("sha256 must be an object")
    if isinstance(assets, dict) and isinstance(hashes, dict):
        for asset in REQUIRED_ASSETS:
            raw_path = require_string(assets.get(asset), f"assets.{asset}", errors)
            expected_hash = require_string(hashes.get(asset), f"sha256.{asset}", errors).upper()
            if not raw_path:
                continue
            path = resolve_path(manifest_path, raw_path)
            paths[asset] = path
            if not path.is_file():
                errors.append(f"missing asset {asset}: {path}")
                continue
            actual_hash = sha256_file(path)
            observed[asset] = actual_hash
            if expected_hash and actual_hash != expected_hash:
                errors.append(f"sha256 mismatch for {asset}")

    assertions = manifest.get("log_assertions")
    if not isinstance(assertions, dict):
        errors.append("log_assertions must be an object")
    else:
        for report in ("map_log", "pnr_log", "sta_log"):
            if report in paths and paths[report].is_file():
                check_text_contains(paths[report], assertions.get(report), report, errors)

    for asset in ("soc_h", "linker", "startup"):
        if asset in paths and paths[asset].is_file():
            content = paths[asset].read_text(encoding="utf-8", errors="replace")
            for token in PLACEHOLDER_TOKENS:
                if token in content:
                    errors.append(f"forbidden provisional token in {asset}: {token}")

    hello_text = require_string(manifest.get("cpu_hello_text"), "cpu_hello_text", errors)
    hello_runs = manifest.get("hello_runs")
    if not isinstance(hello_runs, list) or len(hello_runs) != 3:
        errors.append("hello_runs must contain exactly three reset-run records")
    elif hello_text and build_id:
        for index, run in enumerate(hello_runs, start=1):
            if not isinstance(run, dict):
                errors.append(f"hello_runs[{index}] must be an object")
                continue
            raw_path = require_string(run.get("log"), f"hello_runs[{index}].log", errors)
            expected_hash = require_string(run.get("sha256"), f"hello_runs[{index}].sha256", errors).upper()
            if not raw_path:
                continue
            path = resolve_path(manifest_path, raw_path)
            if not path.is_file():
                errors.append(f"missing hello log {index}: {path}")
                continue
            actual_hash = sha256_file(path)
            observed[f"hello_{index}"] = actual_hash
            if expected_hash and actual_hash != expected_hash:
                errors.append(f"sha256 mismatch for hello log {index}")
            text = path.read_text(encoding="utf-8", errors="replace")
            if hello_text not in text:
                errors.append(f"CPU Hello text missing from reset run {index}")
            if build_id not in text:
                errors.append(f"build_id missing from reset run {index}")

    return {
        "schema": SCHEMA,
        "manifest": str(manifest_path.resolve()),
        "build_id": build_id,
        "result": "PASS" if not errors else "FAIL",
        "errors": errors,
        "observed_sha256": observed,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--report", type=Path, help="write JSON verification evidence to this path")
    args = parser.parse_args()

    report = verify(args.manifest)
    serialized = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(serialized, encoding="utf-8")
    print(serialized, end="")
    return 0 if report["result"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
