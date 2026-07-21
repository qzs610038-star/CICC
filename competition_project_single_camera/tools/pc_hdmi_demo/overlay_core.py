"""Pixel-only foreground detection and overlay helpers for the PC demo.

This module deliberately has no serial, MMIO, robot, or board-control imports.
"""

from __future__ import annotations

from collections import Counter, deque
from dataclasses import dataclass
from typing import Deque, Optional, Tuple

import cv2
import numpy as np


BBox = Tuple[int, int, int, int]


@dataclass(frozen=True)
class Detection:
    bbox: BBox
    color: str
    shape: str
    size: str
    area: float
    confidence: float

    @property
    def label(self) -> str:
        return f"{self.color} | {self.shape} | {self.size}"


class LabelStabilizer:
    """Return a majority label over a short, bounded frame window."""

    def __init__(self, window: int = 5) -> None:
        if window < 1:
            raise ValueError("window must be positive")
        self._labels: Deque[str] = deque(maxlen=window)

    def update(self, detection: Optional[Detection]) -> str:
        label = detection.label if detection is not None else "NO TARGET"
        self._labels.append(label)
        return Counter(self._labels).most_common(1)[0][0]

    def reset(self) -> None:
        self._labels.clear()


def make_synthetic_background(width: int = 640, height: int = 360) -> np.ndarray:
    return np.full((height, width, 3), 127, dtype=np.uint8)


def make_synthetic_frame(
    kind: str,
    width: int = 640,
    height: int = 360,
    offset_x: int = 0,
) -> np.ndarray:
    """Build deterministic scenes for offline tests and no-board demonstrations."""

    frame = make_synthetic_background(width, height)
    cx = width // 2 + offset_x
    cy = height // 2
    colors = {
        "red_cube": (0, 0, 255),
        "blue_cylinder": (255, 0, 0),
        "yellow_cone": (0, 255, 255),
        "white_cube": (245, 245, 245),
        "black_cube": (10, 10, 10),
    }
    if kind == "none":
        return frame
    if kind not in colors:
        raise ValueError(f"unknown synthetic object: {kind}")

    color = colors[kind]
    if kind.endswith("cylinder"):
        cv2.circle(frame, (cx, cy), 62, color, thickness=-1)
    elif kind.endswith("cone"):
        points = np.array(
            [[cx, cy - 75], [cx - 72, cy + 65], [cx + 72, cy + 65]],
            dtype=np.int32,
        )
        cv2.fillPoly(frame, [points], color)
    else:
        cv2.rectangle(frame, (cx - 65, cy - 65), (cx + 65, cy + 65), color, -1)
    return frame


def build_foreground_mask(
    frame: np.ndarray,
    background: np.ndarray,
    diff_threshold: int = 25,
) -> np.ndarray:
    if frame is None or background is None:
        raise ValueError("frame and background are required")
    if frame.shape != background.shape:
        raise ValueError("frame and background dimensions differ")

    delta = cv2.absdiff(frame, background)
    gray = cv2.cvtColor(delta, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (5, 5), 0)
    _, mask = cv2.threshold(gray, diff_threshold, 255, cv2.THRESH_BINARY)
    kernel = np.ones((5, 5), dtype=np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
    return cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)


def _classify_color(frame: np.ndarray, contour: np.ndarray) -> str:
    contour_mask = np.zeros(frame.shape[:2], dtype=np.uint8)
    cv2.drawContours(contour_mask, [contour], -1, 255, thickness=-1)
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    pixels = hsv[contour_mask > 0]
    if pixels.size == 0:
        return "UNKNOWN"

    hue, saturation, value = np.median(pixels, axis=0)
    if value < 55:
        return "BLACK"
    if saturation < 45 and value > 175:
        return "WHITE"
    if hue < 10 or hue > 170:
        return "RED"
    if 15 <= hue <= 40:
        return "YELLOW"
    if 90 <= hue <= 140:
        return "BLUE"
    return "UNKNOWN"


def _classify_shape(contour: np.ndarray) -> str:
    perimeter = cv2.arcLength(contour, True)
    if perimeter <= 0:
        return "UNKNOWN"
    vertices = len(cv2.approxPolyDP(contour, 0.04 * perimeter, True))
    if vertices == 3:
        return "CONE"
    if vertices == 4:
        return "CUBE"
    area = cv2.contourArea(contour)
    circularity = 4.0 * np.pi * area / (perimeter * perimeter)
    return "CYLINDER" if circularity >= 0.68 else "UNKNOWN"


def _classify_size(bbox: BBox, frame_height: int) -> str:
    _, _, width, height = bbox
    ratio = max(width, height) / max(frame_height, 1)
    if ratio < 0.28:
        return "2.0 CM"
    if ratio < 0.42:
        return "2.5 CM"
    return "3.0 CM"


def find_largest_detection(
    frame: np.ndarray,
    background: np.ndarray,
    min_area: float = 900.0,
    diff_threshold: int = 25,
) -> Optional[Detection]:
    mask = build_foreground_mask(frame, background, diff_threshold)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None

    contour = max(contours, key=cv2.contourArea)
    area = float(cv2.contourArea(contour))
    if area < min_area:
        return None

    bbox = tuple(int(value) for value in cv2.boundingRect(contour))
    frame_area = float(frame.shape[0] * frame.shape[1])
    confidence = min(1.0, area / max(frame_area * 0.08, 1.0))
    return Detection(
        bbox=bbox,
        color=_classify_color(frame, contour),
        shape=_classify_shape(contour),
        size=_classify_size(bbox, frame.shape[0]),
        area=area,
        confidence=confidence,
    )


def render_overlay(
    frame: np.ndarray,
    detection: Optional[Detection],
    stable_label: str,
    source_label: str,
    fps: float,
    background_ready: bool,
) -> np.ndarray:
    canvas = frame.copy()
    banner = "PC VISUAL DEMO | ARM DISABLED | BOARD RECOGNITION NOT VERIFIED"
    cv2.rectangle(canvas, (0, 0), (canvas.shape[1], 42), (20, 20, 20), -1)
    cv2.putText(canvas, banner, (12, 27), cv2.FONT_HERSHEY_SIMPLEX, 0.56,
                (0, 220, 255), 2, cv2.LINE_AA)

    if detection is not None:
        x, y, width, height = detection.bbox
        cv2.rectangle(canvas, (x, y), (x + width, y + height), (0, 255, 0), 3)
        cv2.putText(canvas, stable_label, (x, max(62, y - 10)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.72, (0, 255, 0), 2, cv2.LINE_AA)
    else:
        cv2.putText(canvas, stable_label, (18, 78), cv2.FONT_HERSHEY_SIMPLEX,
                    0.72, (0, 180, 255), 2, cv2.LINE_AA)

    status = "READY" if background_ready else "CAPTURE EMPTY BACKGROUND: press B"
    footer = f"SOURCE={source_label} | FPS={fps:.1f} | {status}"
    cv2.rectangle(canvas, (0, canvas.shape[0] - 34),
                  (canvas.shape[1], canvas.shape[0]), (20, 20, 20), -1)
    cv2.putText(canvas, footer, (12, canvas.shape[0] - 11),
                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (240, 240, 240), 1, cv2.LINE_AA)
    return canvas
