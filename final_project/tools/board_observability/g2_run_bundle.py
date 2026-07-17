#!/usr/bin/env python3
"""Offline-only G2 evidence bundle creator and verifier.

This tool never opens a serial port, invokes a programmer, or communicates with
hardware. It converts deterministic Host/fake-transport event lines into a
small, self-contained evidence bundle.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REQUIRED_FIELDS = (
    "v", "seq", "event", "round", "frame", "cfg", "flags", "class",
    "decision", "reason", "ack", "arm", "source",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_event(line: str) -> dict[str, str]:
    if not line.startswith("@E|"):
        raise ValueError("not an event")
    fields: dict[str, str] = {}
    for part in line.rstrip("\r\n").split("|")[1:]:
        if "=" not in part:
            raise ValueError("malformed key-value token")
        key, value = part.split("=", 1)
        if not key or key in fields:
            raise ValueError("duplicate or empty key")
        fields[key] = value
    missing = [key for key in REQUIRED_FIELDS if key not in fields]
    if missing:
        raise ValueError("missing fields: " + ",".join(missing))
    if fields["v"] != "1":
        raise ValueError("unsupported schema version")
    for number in ("seq", "round", "frame", "cfg", "flags", "ack", "arm"):
        if not fields[number].isdigit():
            raise ValueError("non-numeric " + number)
    return fields


def git_value(repo_root: Path, *args: str) -> str:
    try:
        return subprocess.check_output(["git", *args], cwd=repo_root,
                                       text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "UNKNOWN"


def test_summary(raw_lines: list[str]) -> tuple[int, int, int]:
    for line in reversed(raw_lines):
        if line.startswith("TEST_SUMMARY "):
            pieces = dict(token.split("=", 1) for token in line.split()[1:] if "=" in token)
            return int(pieces["total"]), int(pieces["passed"]), int(pieces["failures"])
    return 0, 0, 0


def create_bundle(args: argparse.Namespace) -> int:
    run_dir = Path(args.run_dir).resolve()
    raw_log = Path(args.raw_log).resolve()
    repo_root = Path(args.repo_root).resolve()
    run_dir.mkdir(parents=True, exist_ok=True)
    if not raw_log.is_file():
        raise ValueError("raw.log does not exist")
    raw_target = run_dir / "raw.log"
    if raw_target != raw_log:
        raw_target.write_bytes(raw_log.read_bytes())
    raw_lines = raw_target.read_text(encoding="utf-8", errors="replace").splitlines()
    events: list[dict[str, str]] = []
    malformed: list[dict[str, str]] = []
    for number, line in enumerate(raw_lines, 1):
        if line.startswith("@E"):
            try:
                event = parse_event(line)
                event["raw_line"] = str(number)
                events.append(event)
            except ValueError as exc:
                malformed.append({"raw_line": str(number), "error": str(exc), "raw": line})
    (run_dir / "events.jsonl").write_text(
        "".join(json.dumps(event, sort_keys=True) + "\n" for event in events), encoding="utf-8"
    )
    total, passed, failures = test_summary(raw_lines)
    inputs = {"raw.log": sha256(raw_target)}
    for relative in (
        "competition_project_single_camera/cpu/src/single_camera_runtime.c",
        "competition_project_single_camera/cpu/src/single_camera_fake_transport.c",
        "competition_project_single_camera/cpu/tests/test_single_camera_runtime.c",
    ):
        candidate = repo_root / relative
        if candidate.is_file():
            inputs[relative] = sha256(candidate)
    test_command = args.test_command
    test_command_base64 = getattr(args, "test_command_base64", None)
    if test_command_base64:
        test_command = base64.b64decode(test_command_base64, validate=True).decode("utf-8")
    if not test_command:
        raise ValueError("test command is required")
    manifest = {
        "schema_version": 1,
        "goal": "G2",
        "git_head": git_value(repo_root, "rev-parse", "HEAD"),
        "git_dirty": bool(git_value(repo_root, "status", "--porcelain")),
        "repo_root": str(repo_root),
        "compiler": args.compiler,
        "test_command": test_command,
        "exit_code": args.exit_code,
        "input_sha256": inputs,
        "test_total": total,
        "assertions_passed": passed,
        "assertions_failed": failures,
        "event_count": len(events),
        "malformed_event_count": len(malformed),
        "ack_count": sum(1 for event in events if event["event"] == "ACK"),
        "arm_request_count": 0,
        "arm_send_count": 0,
        "source": "host_fixture_or_fake_transport",
        "board_not_verified": True,
        "risc_v_elf_not_built": True,
        "fpga_apb_not_implemented": True,
        "arm_disabled": True,
    }
    (run_dir / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                                             encoding="utf-8")
    (run_dir / "summary.md").write_text(
        "# G2 offline run bundle\n\n"
        f"- Result: HOST/FAKE TRANSPORT only; exit code `{args.exit_code}`.\n"
        f"- Assertions: `{passed}/{total}` passed; failures `{failures}`.\n"
        f"- Events: `{len(events)}`; ACK: `{manifest['ack_count']}`; malformed: `{len(malformed)}`.\n"
        "- ARM requests/sends: `0/0`; ARM remains disabled.\n"
        "- BOARD_NOT_VERIFIED; RISC_V_ELF_NOT_BUILT; FPGA_APB_NOT_IMPLEMENTED.\n",
        encoding="utf-8",
    )
    return 0


def validate_bundle(run_dir: Path) -> list[str]:
    errors: list[str] = []
    manifest_path = run_dir / "manifest.json"
    raw_path = run_dir / "raw.log"
    events_path = run_dir / "events.jsonl"
    if not all(path.is_file() for path in (manifest_path, raw_path, events_path, run_dir / "summary.md")):
        return ["bundle missing required file"]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    raw_lines = raw_path.read_text(encoding="utf-8", errors="replace").splitlines()
    event_lines = [line for line in raw_lines if line.startswith("@E")]
    parsed_raw: list[dict[str, str]] = []
    for line in event_lines:
        try:
            parsed_raw.append(parse_event(line))
        except ValueError as exc:
            errors.append("malformed event: " + str(exc))
    json_events = [json.loads(line) for line in events_path.read_text(encoding="utf-8").splitlines() if line]
    if len(parsed_raw) != len(json_events):
        errors.append("events.jsonl does not preserve parsed raw events")
    if not parsed_raw or parsed_raw[0]["event"] != "BOOT":
        errors.append("missing startup BOOT event")
    previous = 0
    accepted: set[str] = set()
    acknowledged: list[str] = []
    round_results: set[tuple[str, str]] = set()
    for event in parsed_raw:
        sequence = int(event["seq"])
        if sequence != previous + 1:
            errors.append("event sequence is not contiguous")
        previous = sequence
        if event["source"] not in ("fake_transport", "host_fixture"):
            errors.append("unexpected event source")
        if event["arm"] != "0":
            errors.append("arm_enabled is non-zero")
        if event["event"] == "SNAPSHOT_ACCEPT":
            accepted.add(event["frame"])
        if event["event"] == "ACK":
            acknowledged.append(event["frame"])
        if event["event"] == "ROUND_RESULT":
            key = (event["round"], event["frame"])
            if key in round_results:
                errors.append("duplicate ROUND_RESULT")
            round_results.add(key)
    if set(acknowledged) != accepted or len(acknowledged) != len(set(acknowledged)):
        errors.append("missing or mismatched ACK")
    if manifest.get("ack_count") != len(acknowledged):
        errors.append("manifest ACK count mismatch")
    if manifest.get("arm_request_count") != 0 or manifest.get("arm_send_count") != 0:
        errors.append("manifest arm count is non-zero")
    if manifest.get("source") != "host_fixture_or_fake_transport":
        errors.append("manifest source boundary missing")
    if not manifest.get("board_not_verified") or not manifest.get("arm_disabled"):
        errors.append("verification boundary missing")
    repo_root = Path(manifest.get("repo_root", ""))
    for relative, expected in manifest.get("input_sha256", {}).items():
        path = raw_path if relative == "raw.log" else repo_root / relative
        if not path.is_file() or sha256(path) != expected:
            errors.append("mixed hash")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="G2 offline-only run bundle tool")
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create")
    create.add_argument("--run-dir", required=True)
    create.add_argument("--raw-log", required=True)
    create.add_argument("--repo-root", required=True)
    create.add_argument("--compiler", required=True)
    create.add_argument("--test-command")
    create.add_argument("--test-command-base64")
    create.add_argument("--exit-code", type=int, required=True)
    validate = sub.add_parser("validate")
    validate.add_argument("--run-dir", required=True)
    args = parser.parse_args()
    try:
        if args.command == "create":
            return create_bundle(args)
        errors = validate_bundle(Path(args.run_dir).resolve())
        if errors:
            print("VALIDATION_FAIL: " + "; ".join(errors))
            return 1
        print("VALIDATION_PASS: offline bundle")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print("BUNDLE_ERROR: " + str(exc))
        return 2


if __name__ == "__main__":
    sys.exit(main())
