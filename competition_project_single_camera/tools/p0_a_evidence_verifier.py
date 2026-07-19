#!/usr/bin/env python3
"""Fail-closed verifier for a P0-A diagnostic-firmware evidence bundle.

This tool deliberately does not select a compiler, linker, objdump, or file
layout.  The firmware owner supplies those artifacts and their hashes in a
manifest; this verifier then independently reads the ELF and checks the
evidence relationships needed before P0-A can be considered READY.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any


class VerificationError(Exception):
    pass


def fail(message: str) -> None:
    raise VerificationError(message)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def as_int(value: Any, field: str) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:
            raise VerificationError(f"{field}: not an integer: {value!r}") from exc
    fail(f"{field}: expected integer or hexadecimal string")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def bundle_file(bundle_root: Path, relative: str, label: str) -> Path:
    candidate = (bundle_root / relative).resolve()
    try:
        candidate.relative_to(bundle_root)
    except ValueError as exc:
        raise VerificationError(f"{label}: path escapes bundle root: {relative}") from exc
    require(candidate.is_file(), f"{label}: missing file: {relative}")
    return candidate


def artifact(manifest: dict[str, Any], bundle_root: Path, name: str) -> Path:
    entry = manifest["artifacts"].get(name)
    require(isinstance(entry, dict), f"artifacts.{name}: missing object")
    path = bundle_file(bundle_root, str(entry.get("path", "")), f"artifacts.{name}")
    expected = str(entry.get("sha256", "")).upper()
    require(len(expected) == 64 and all(ch in "0123456789ABCDEF" for ch in expected),
            f"artifacts.{name}: sha256 must be 64 hexadecimal characters")
    actual = sha256(path)
    require(actual == expected, f"artifacts.{name}: SHA-256 mismatch expected={expected} actual={actual}")
    return path


def parse_elf(path: Path) -> dict[str, Any]:
    raw = path.read_bytes()
    require(raw[:4] == b"\x7fELF", "ELF: magic is missing")
    elf_class = raw[4]
    data_encoding = raw[5]
    require(elf_class in (1, 2), f"ELF: unsupported class {elf_class}")
    require(data_encoding in (1, 2), f"ELF: unsupported data encoding {data_encoding}")
    endian = "<" if data_encoding == 1 else ">"
    header_format = endian + ("16sHHIIIIIHHHHHH" if elf_class == 1 else "16sHHIQQQIHHHHHH")
    require(len(raw) >= struct.calcsize(header_format), "ELF: truncated file header")
    header = struct.unpack_from(header_format, raw)
    entry = header[4]
    phoff, shoff = header[5], header[6]
    phentsize, phnum, shentsize, shnum = header[9], header[10], header[11], header[12]
    expected_phentsize = 32 if elf_class == 1 else 56
    expected_shentsize = 40 if elf_class == 1 else 64
    require(phentsize == expected_phentsize, "ELF: unexpected program-header entry size")
    require(shentsize == expected_shentsize, "ELF: unexpected section-header entry size")
    require(phoff + phentsize * phnum <= len(raw), "ELF: program headers are truncated")
    require(shoff + shentsize * shnum <= len(raw), "ELF: section headers are truncated")
    load_segments: list[tuple[int, int]] = []
    ph_format = endian + ("IIIIIIII" if elf_class == 1 else "IIQQQQQQ")
    for index in range(phnum):
        item = struct.unpack_from(ph_format, raw, phoff + index * phentsize)
        p_type = item[0]
        p_vaddr, p_memsz = (item[2], item[5]) if elf_class == 1 else (item[3], item[6])
        if p_type == 1 and p_memsz:
            load_segments.append((p_vaddr, p_vaddr + p_memsz))

    section_headers: list[tuple[int, ...]] = []
    sh_format = endian + ("IIIIIIIIII" if elf_class == 1 else "IIQQQQIIQQ")
    for index in range(shnum):
        section_headers.append(struct.unpack_from(sh_format, raw, shoff + index * shentsize))
    symbols: dict[str, int] = {}
    for section in section_headers:
        sh_type = section[1]
        sh_offset, sh_size, sh_link, sh_entsize = (section[4], section[5], section[6], section[9])
        if sh_type not in (2, 11) or sh_entsize == 0 or sh_link >= len(section_headers):
            continue
        string_section = section_headers[sh_link]
        str_offset, str_size = string_section[4], string_section[5]
        require(str_offset + str_size <= len(raw), "ELF: symbol string table is truncated")
        strings = raw[str_offset:str_offset + str_size]
        require(sh_offset + sh_size <= len(raw), "ELF: symbol table is truncated")
        symbol_format = endian + ("IIIBBH" if elf_class == 1 else "IBBHQQ")
        for offset in range(sh_offset, sh_offset + sh_size, sh_entsize):
            if offset + struct.calcsize(symbol_format) > len(raw):
                fail("ELF: malformed symbol entry")
            entry_data = struct.unpack_from(symbol_format, raw, offset)
            name_offset, value = entry_data[0], entry_data[1] if elf_class == 1 else entry_data[4]
            if name_offset >= len(strings):
                continue
            end = strings.find(b"\0", name_offset)
            if end < 0:
                continue
            name = strings[name_offset:end].decode("utf-8", "replace")
            if name:
                symbols[name] = value
    return {"entry": entry, "loads": load_segments, "symbols": symbols}


def inside(start: int, end: int, base: int, limit: int) -> bool:
    return base <= start <= end <= limit


def overlaps(left: tuple[int, int], right: tuple[int, int]) -> bool:
    return left[0] < right[1] and right[0] < left[1]


def verify_memory_layout(manifest: dict[str, Any], elf: dict[str, Any]) -> None:
    ram = manifest["memory"]
    base = as_int(ram.get("base"), "memory.base")
    size = as_int(ram.get("size_bytes"), "memory.size_bytes")
    require(size == 16 * 1024, "memory.size_bytes: P0-A requires exactly 16384 bytes")
    limit = base + size
    require(inside(elf["entry"], elf["entry"] + 1, base, limit),
            f"ELF entry 0x{elf['entry']:X} is outside 16 KiB RAM")
    require(elf["loads"], "ELF: no non-empty PT_LOAD segment")
    for start, end in elf["loads"]:
        require(inside(start, end, base, limit),
                f"ELF LOAD segment [0x{start:X},0x{end:X}) is outside 16 KiB RAM")

    canary = manifest["canary"]
    symbol_name = str(canary.get("symbol", ""))
    canary_address = elf["symbols"].get(symbol_name)
    require(canary_address is not None, f"ELF: canary symbol not found: {symbol_name!r}")
    canary_size = as_int(canary.get("size_bytes"), "canary.size_bytes")
    canary_range = (canary_address, canary_address + canary_size)
    require(inside(*canary_range, base, limit), "canary: range is outside 16 KiB RAM")

    regions: list[tuple[str, tuple[int, int]]] = []
    for index, region in enumerate(manifest["memory"].get("regions", [])):
        name = str(region.get("name", f"region_{index}"))
        start = as_int(region.get("start"), f"memory.regions[{index}].start")
        end = as_int(region.get("end"), f"memory.regions[{index}].end")
        require(start < end, f"memory.regions[{index}]: start must be lower than end")
        require(inside(start, end, base, limit), f"memory.regions[{index}]: outside 16 KiB RAM")
        regions.append((name, (start, end)))
    require(any(name == "stack" for name, _ in regions), "memory.regions: explicit stack region is required")
    require(any(name == "canary" and value == canary_range for name, value in regions),
            "memory.regions: explicit canary region must equal ELF symbol address and size")
    for index, (left_name, left_range) in enumerate(regions):
        for right_name, right_range in regions[index + 1:]:
            require(not overlaps(left_range, right_range),
                    f"memory.regions: overlap between {left_name} and {right_name}")


def verify_text_evidence(manifest: dict[str, Any], files: dict[str, Path], elf: dict[str, Any]) -> None:
    map_text = files["map"].read_text(encoding="utf-8", errors="replace")
    readelf_text = files["readelf"].read_text(encoding="utf-8", errors="replace")
    require(str(manifest["canary"]["symbol"]) in map_text, "map evidence does not name the canary symbol")
    require("LOAD" in readelf_text and "Entry point" in readelf_text,
            "readelf evidence must contain LOAD program-header and entry-point output")
    entry_hex = f"{elf['entry']:x}"
    require(entry_hex in readelf_text.lower(), "readelf evidence does not contain parsed ELF entry address")
    order = manifest["disassembly_order_witness"]
    require(order.get("artifact") == "objdump", "disassembly witness must refer to objdump artifact")
    canary_offsets = [as_int(v, "disassembly_order_witness.canary_write_offsets") for v in order.get("canary_write_offsets", [])]
    wait_offsets = [as_int(v, "disassembly_order_witness.uart_wait_offsets") for v in order.get("uart_wait_offsets", [])]
    require(canary_offsets and wait_offsets, "disassembly witness requires canary and UART-wait offsets")
    require(max(canary_offsets) < min(wait_offsets), "disassembly witness does not prove canary writes precede UART wait")
    objdump_text = files["objdump"].read_text(encoding="utf-8", errors="replace")
    for offset in canary_offsets + wait_offsets:
        require(f"{offset:x}" in objdump_text.lower(), f"objdump evidence misses witness offset 0x{offset:X}")


def verify_negative_case(manifest: dict[str, Any], files: dict[str, Path]) -> None:
    evidence = json.loads(files["tx_never_ready"].read_text(encoding="utf-8"))
    require(evidence.get("case_id") == "P0-A-TX-NEVER-READY", "negative evidence: unexpected case_id")
    require(evidence.get("status") == "PASS" and evidence.get("runner_exit_code") == 0,
            "negative evidence: test did not pass with exit 0")
    require(evidence.get("observed_stage") == "E101", "negative evidence: TX timeout did not reach E101")
    samples = evidence.get("uart_ready_samples")
    require(isinstance(samples, list) and samples and not any(samples),
            "negative evidence: TX-ready was not continuously false")
    polls = as_int(evidence.get("observed_poll_count"), "negative evidence.observed_poll_count")
    bound = as_int(evidence.get("bounded_poll_limit"), "negative evidence.bounded_poll_limit")
    require(0 < polls <= bound, "negative evidence: UART poll count is not bounded")
    before = as_int(evidence.get("heartbeat_before_timeout"), "negative evidence.heartbeat_before_timeout")
    after = as_int(evidence.get("heartbeat_after_timeout"), "negative evidence.heartbeat_after_timeout")
    require(after > before, "negative evidence: heartbeat did not continue after TX timeout")


def verify_stop_conditions(manifest: dict[str, Any]) -> None:
    stop = manifest["stop_conditions"]
    require(as_int(stop.get("same_failure_max_attempts"), "stop_conditions.same_failure_max_attempts") == 2,
            "stop conditions: same failure limit must be two attempts")
    require(stop.get("requires_new_evidence_per_attempt") is True,
            "stop conditions: new evidence per attempt must be true")
    require(stop.get("third_attempt_requires_review") is True,
            "stop conditions: third attempt must require review")
    safety = manifest["safety"]
    require(safety.get("arm_enabled") == 0, "safety.arm_enabled must remain 0")
    forbidden = set(safety.get("forbidden", []))
    require({"UART2/J52", "myCobot"}.issubset(forbidden),
            "safety.forbidden must include UART2/J52 and myCobot")


def verify(manifest_path: Path) -> None:
    bundle_root = manifest_path.parent.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("schema") == "p0-a-evidence-v1", "schema must be p0-a-evidence-v1")
    firmware = manifest.get("firmware")
    require(isinstance(firmware, dict), "firmware: missing object")
    git_sha = str(firmware.get("git_sha", ""))
    require(len(git_sha) == 40 and all(ch in "0123456789abcdef" for ch in git_sha),
            "firmware.git_sha must be a lowercase 40-character SHA")
    input_hash = str(firmware.get("input_sha256", "")).upper()
    require(len(input_hash) == 64 and all(ch in "0123456789ABCDEF" for ch in input_hash),
            "firmware.input_sha256 must be a SHA-256")
    require(isinstance(manifest.get("artifacts"), dict), "artifacts: missing object")
    files = {name: artifact(manifest, bundle_root, name)
             for name in ("elf", "map", "readelf", "objdump", "build_log", "tx_never_ready")}
    elf = parse_elf(files["elf"])
    verify_memory_layout(manifest, elf)
    verify_text_evidence(manifest, files, elf)
    verify_negative_case(manifest, files)
    verify_stop_conditions(manifest)
    print("P0_A_EVIDENCE=PASS")
    print(f"firmware_git_sha={git_sha}")
    print(f"elf_entry=0x{elf['entry']:X}")
    print(f"canary_symbol={manifest['canary']['symbol']} address=0x{elf['symbols'][manifest['canary']['symbol']]:X}")
    print("board_status=NOT_VERIFIED")


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify a P0-A diagnostic-firmware evidence manifest")
    parser.add_argument("manifest", type=Path, help="bundle-local p0-a-evidence-v1 JSON manifest")
    args = parser.parse_args()
    try:
        verify(args.manifest.resolve())
    except (OSError, ValueError, KeyError, VerificationError, json.JSONDecodeError) as exc:
        print(f"P0_A_EVIDENCE=FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
