#!/usr/bin/env python3
"""Extract RTL-equivalent preboard statistics from one RGB image.

The image is processed in two-pixel (p0 then p1) order to mirror
feature_stats_tap.v.  Output is explicitly HOST_CALIBRATION_PROVISIONAL.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - exercised by the operational gate
    Image = None


SCHEMA = "QW-CALIBRATION-SAMPLE-v1"
FLAGS = 0x47
MAX_COUNT = (1 << 21) - 1
MAX_LUMA = (1 << 31) - 1


class ExtractError(Exception):
    pass


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def rgb48_pair(p0: tuple[int, int, int], p1: tuple[int, int, int]) -> int:
    """Pack p0=[47:24], p1=[23:0], exactly as the frozen tap contract."""
    return (p0[0] << 40) | (p0[1] << 32) | (p0[2] << 24) | (p1[0] << 16) | (p1[1] << 8) | p1[2]


def unpack_rgb48_pair(word: int) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    return ((word >> 40 & 0xff, word >> 32 & 0xff, word >> 24 & 0xff), (word >> 16 & 0xff, word >> 8 & 0xff, word & 0xff))


def is_red(rgb: tuple[int, int, int], thresholds: dict) -> bool:
    r, g, b = rgb
    return r >= thresholds["red_min"] and r >= g + thresholds["color_delta"] and r >= b + thresholds["color_delta"]


def is_blue(rgb: tuple[int, int, int], thresholds: dict) -> bool:
    r, g, b = rgb
    return b >= thresholds["blue_min"] and b >= r + thresholds["color_delta"] and b >= g + thresholds["color_delta"]


def is_yellow(rgb: tuple[int, int, int], thresholds: dict) -> bool:
    r, g, b = rgb
    return (r >= thresholds["yellow_min"] and g >= thresholds["yellow_min"] and abs(r - g) <= thresholds["color_delta"] and min(r, g) >= b + thresholds["color_delta"])


def is_foreground(rgb: tuple[int, int, int], background: dict, thresholds: dict) -> bool:
    r, g, b = rgb
    return abs(r - background["r"]) + abs(g - background["g"]) + abs(b - background["b"]) >= thresholds["foreground_delta"]


def get_required(metadata: dict, name: str):
    if name not in metadata:
        raise ExtractError(f"metadata missing {name}")
    return metadata[name]


def extract(image_path: Path, profile: dict, metadata: dict) -> dict:
    if Image is None:
        raise ExtractError("DEPENDENCY_BLOCKED: Pillow is unavailable; do not substitute an algorithm")
    if profile.get("schema") != SCHEMA or profile.get("camera_route") != "J48/ch0":
        raise ExtractError("profile must be a QW-CALIBRATION-SAMPLE-v1 J48/ch0 profile")
    try:
        image = Image.open(image_path).convert("RGB")
    except Exception as exc:
        raise ExtractError(f"cannot decode RGB image: {exc}") from exc
    width, height = image.size
    if (width, height) != (profile.get("frame_width"), profile.get("frame_height")):
        raise ExtractError("image dimensions must match the fixed capture profile")
    roi = profile["roi"]
    if not (0 <= roi["x0"] <= roi["x1"] < width and 0 <= roi["y0"] <= roi["y1"] < height):
        raise ExtractError("profile ROI is not a closed in-image rectangle")
    thresholds = profile["thresholds"]
    background = profile["background"]
    pixels = image.load()
    counts = {"red_area": 0, "blue_area": 0, "yellow_area": 0, "foreground_area": 0, "roi_pixel_count": 0, "sum_luma": 0}
    min_x = min_y = None
    max_x = max_y = None
    for y in range(height):
        for x in range(0, width, 2):
            p0 = pixels[x, y]
            p1 = pixels[x + 1, y] if x + 1 < width else (0, 0, 0)
            # The pack/unpack round trip guards the 48-bit p0/p1 byte ordering.
            p0, p1 = unpack_rgb48_pair(rgb48_pair(p0, p1))
            for px, rgb in ((x, p0), (x + 1, p1)):
                if px >= width or not (roi["x0"] <= px <= roi["x1"] and roi["y0"] <= y <= roi["y1"]):
                    continue
                counts["roi_pixel_count"] += 1
                counts["sum_luma"] += sum(rgb)
                counts["red_area"] += int(is_red(rgb, thresholds))
                counts["blue_area"] += int(is_blue(rgb, thresholds))
                counts["yellow_area"] += int(is_yellow(rgb, thresholds))
                foreground = is_foreground(rgb, background, thresholds)
                counts["foreground_area"] += int(foreground)
                if foreground:
                    min_x = px if min_x is None else min(min_x, px)
                    max_x = px if max_x is None else max(max_x, px)
                    min_y = y if min_y is None else min(min_y, y)
                    max_y = y if max_y is None else max(max_y, y)
    if any(value > MAX_COUNT for name, value in counts.items() if name != "sum_luma") or counts["sum_luma"] > MAX_LUMA:
        raise ExtractError("counter overflow: HOST result cannot represent the frozen field width")
    bbox_width = 0 if min_x is None else max_x - min_x + 1
    bbox_height = 0 if min_y is None else max_y - min_y + 1
    row = {
        "schema": SCHEMA,
        "batch_id": get_required(metadata, "batch_id"),
        "sample_id": get_required(metadata, "sample_id"),
        "object_id": get_required(metadata, "object_id"),
        "artifact_sha256": digest(image_path),
        "evidence_level": "HOST_CALIBRATION_PROVISIONAL",
        "color": get_required(metadata, "color"),
        "shape": get_required(metadata, "shape"),
        "nominal_size_cm_x10": get_required(metadata, "nominal_size_cm_x10"),
        "lighting_id": get_required(metadata, "lighting_id"),
        "position_id": get_required(metadata, "position_id"),
        "capture_profile_id": profile["capture_profile_id"],
        "frame_id": get_required(metadata, "frame_id"),
        "config_seq": get_required(metadata, "config_seq"),
        "source_flags": FLAGS,
        **counts,
        "bbox_width": bbox_width,
        "bbox_height": bbox_height,
        "expected_use": get_required(metadata, "expected_use"),
    }
    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--capture-profile", required=True, type=Path)
    parser.add_argument("--metadata", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        profile = json.loads(args.capture_profile.read_text(encoding="utf-8"))
        metadata = json.loads(args.metadata.read_text(encoding="utf-8"))
        row = extract(args.image, profile, metadata)
        args.output.write_text(json.dumps(row, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    except (OSError, ValueError, ExtractError) as exc:
        print(json.dumps({"status": "FAIL", "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps({"status": "PASS", "evidence_level": "HOST_CALIBRATION_PROVISIONAL", "output": str(args.output.name)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
