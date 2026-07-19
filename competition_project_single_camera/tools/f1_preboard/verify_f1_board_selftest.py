#!/usr/bin/env python3
"""Verify exactly one fail-closed QW-F1-BOARD-SELFTEST-v1 UART capture line."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


LINE = re.compile(r"^@F1SELFTEST\|v=1\|build=([A-Za-z0-9._-]+)\|cases=([1-9][0-9]*)\|pass=([1-9][0-9]*)\|digest=([0-9a-f]{8,64})\|arm=0$")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--expected-build", required=True)
    parser.add_argument("--expected-cases", required=True, type=int)
    parser.add_argument("--expected-digest", required=True)
    args = parser.parse_args()
    try:
        if not re.fullmatch(r"[0-9a-f]{8,64}", args.expected_digest):
            raise ValueError("expected digest must be lowercase hexadecimal")
        raw = args.capture.read_bytes()
        text = raw.decode("ascii", "strict")
        if not text.endswith("\n") or text.count("\n") != 1 or "\r" in text:
            raise ValueError("capture must contain exactly one LF-terminated ASCII summary")
        match = LINE.fullmatch(text[:-1])
        if match is None:
            raise ValueError("capture is malformed, duplicated, garbled, or contains extra fields")
        build, cases, passed, digest = match.groups()
        if build != args.expected_build or int(cases) != args.expected_cases or int(passed) != args.expected_cases or digest != args.expected_digest:
            raise ValueError("summary identity, case count, pass count, or digest mismatch")
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps({"status": "PASS", "board_cpu_f1_selftest": "CAPTURE_FORMAT_VERIFIED_ONLY", "arm": 0}, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
