"""PC-only HDMI/UVC visualization candidate.

The program consumes pixels from a UVC camera, a video file, or a deterministic
synthetic source. It never sends serial, MMIO, or robot commands.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Optional, Tuple

import cv2
import numpy as np

from overlay_core import (
    LabelStabilizer,
    find_largest_detection,
    make_synthetic_background,
    make_synthetic_frame,
    render_overlay,
)


SYNTHETIC_KINDS = (
    "none",
    "red_cube",
    "blue_cylinder",
    "yellow_cone",
    "white_cube",
    "black_cube",
)


def list_camera_devices(max_devices: int = 8) -> int:
    devices = []
    backend = cv2.CAP_DSHOW if os.name == "nt" else cv2.CAP_ANY
    for index in range(max_devices):
        capture = cv2.VideoCapture(index, backend)
        try:
            ok, frame = capture.read() if capture.isOpened() else (False, None)
            if ok and frame is not None:
                devices.append(
                    {
                        "index": index,
                        "width": int(frame.shape[1]),
                        "height": int(frame.shape[0]),
                    }
                )
        finally:
            capture.release()
    print(json.dumps({"schema": "pc-video-device-list-v1", "devices": devices}))
    return 0


def open_capture(args: argparse.Namespace) -> cv2.VideoCapture:
    if args.source == "file":
        if not args.file:
            raise ValueError("--file is required when --source=file")
        capture = cv2.VideoCapture(str(args.file))
    else:
        backend = cv2.CAP_DSHOW if os.name == "nt" else cv2.CAP_ANY
        capture = cv2.VideoCapture(args.device, backend)
        capture.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        capture.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        capture.set(cv2.CAP_PROP_FPS, args.fps)
    if not capture.isOpened():
        raise RuntimeError("VIDEO_SOURCE_UNAVAILABLE")
    return capture


def synthetic_frame(index: int, width: int, height: int) -> np.ndarray:
    phase = (index // 45) % len(SYNTHETIC_KINDS)
    kind = SYNTHETIC_KINDS[phase]
    offset = int(35 * np.sin(index / 11.0))
    return make_synthetic_frame(kind, width, height, offset)


def read_frame(
    args: argparse.Namespace,
    capture: Optional[cv2.VideoCapture],
    frame_index: int,
) -> Tuple[bool, Optional[np.ndarray]]:
    if args.source == "synthetic":
        return True, synthetic_frame(frame_index, args.width, args.height)
    assert capture is not None
    return capture.read()


def write_run_artifacts(
    output_dir: Path,
    summary: dict,
    last_frame: Optional[np.ndarray],
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "run_summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    if last_frame is not None:
        cv2.imwrite(str(output_dir / "last_frame.png"), last_frame)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", choices=("synthetic", "camera", "file"),
                        default="synthetic")
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--file", type=Path)
    parser.add_argument("--source-label", default="")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=float, default=30.0)
    parser.add_argument("--frames", type=int, default=0,
                        help="0 means unlimited in GUI mode or 180 synthetic headless frames")
    parser.add_argument("--min-area", type=float, default=900.0)
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--list-devices", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.list_devices:
        return list_camera_devices()
    if args.width < 160 or args.height < 120 or args.frames < 0:
        print("INVALID_ARGUMENT", file=sys.stderr)
        return 2

    source_label = args.source_label or {
        "synthetic": "SYNTHETIC-OFFLINE",
        "camera": f"UVC-CAMERA-{args.device}",
        "file": "RECORDED-VIDEO",
    }[args.source]

    capture: Optional[cv2.VideoCapture] = None
    if args.source != "synthetic":
        try:
            capture = open_capture(args)
        except (RuntimeError, ValueError) as exc:
            print(str(exc), file=sys.stderr)
            return 3

    background = (
        make_synthetic_background(args.width, args.height)
        if args.source == "synthetic"
        else None
    )
    stabilizer = LabelStabilizer(window=5)
    frame_limit = args.frames
    if args.headless and frame_limit == 0:
        frame_limit = 180 if args.source == "synthetic" else 300

    frame_count = 0
    detection_count = 0
    read_failures = 0
    last_canvas: Optional[np.ndarray] = None
    start = time.perf_counter()
    last_tick = start
    measured_fps = 0.0

    try:
        while frame_limit == 0 or frame_count < frame_limit:
            ok, frame = read_frame(args, capture, frame_count)
            if not ok or frame is None:
                read_failures += 1
                if read_failures >= 5:
                    print("VIDEO_STREAM_LOST", file=sys.stderr)
                    break
                continue
            read_failures = 0
            if frame.shape[1] != args.width or frame.shape[0] != args.height:
                frame = cv2.resize(frame, (args.width, args.height))

            if background is None:
                background = frame.copy()
            detection = find_largest_detection(frame, background, args.min_area)
            if detection is not None:
                detection_count += 1
            stable_label = stabilizer.update(detection)
            now = time.perf_counter()
            if now > last_tick:
                measured_fps = 0.9 * measured_fps + 0.1 / (now - last_tick)
            last_tick = now
            last_canvas = render_overlay(
                frame,
                detection,
                stable_label,
                source_label,
                measured_fps,
                background is not None,
            )
            frame_count += 1

            if args.headless:
                continue
            cv2.imshow("CICC PC HDMI Overlay Candidate", last_canvas)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27):
                break
            if key == ord("b"):
                background = frame.copy()
                stabilizer.reset()
            if key == ord("r"):
                background = None if args.source != "synthetic" else make_synthetic_background(
                    args.width, args.height
                )
                stabilizer.reset()
            if key == ord("s") and args.output_dir:
                args.output_dir.mkdir(parents=True, exist_ok=True)
                cv2.imwrite(str(args.output_dir / f"frame_{frame_count:06d}.png"), last_canvas)
    finally:
        if capture is not None:
            capture.release()
        cv2.destroyAllWindows()

    elapsed = max(time.perf_counter() - start, 1e-9)
    summary = {
        "schema": "pc-hdmi-overlay-run-v1",
        "status": "HOST_SYNTHETIC_PASS" if args.source == "synthetic" else "HOST_SOURCE_RUN",
        "source": args.source,
        "source_label": source_label,
        "frames": frame_count,
        "detections": detection_count,
        "elapsed_seconds": round(elapsed, 3),
        "average_fps": round(frame_count / elapsed, 3),
        "arm_enabled": 0,
        "board_verified": False,
        "hdmi_uvc_verified": False,
    }
    print(json.dumps(summary, sort_keys=True))
    if args.output_dir:
        write_run_artifacts(args.output_dir, summary, last_canvas)
    if frame_count == 0:
        return 4
    if args.source == "synthetic" and detection_count == 0:
        return 5
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
