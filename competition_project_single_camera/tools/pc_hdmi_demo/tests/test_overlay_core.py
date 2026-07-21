from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np


TOOL_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_ROOT))

from overlay_core import (  # noqa: E402
    LabelStabilizer,
    find_largest_detection,
    make_synthetic_background,
    make_synthetic_frame,
    render_overlay,
)


class OverlayCoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.background = make_synthetic_background()

    def test_empty_scene_has_no_detection(self) -> None:
        frame = make_synthetic_frame("none")
        self.assertIsNone(find_largest_detection(frame, self.background))

    def test_synthetic_color_and_shape_labels(self) -> None:
        cases = {
            "red_cube": ("RED", "CUBE"),
            "blue_cylinder": ("BLUE", "CYLINDER"),
            "yellow_cone": ("YELLOW", "CONE"),
            "white_cube": ("WHITE", "CUBE"),
            "black_cube": ("BLACK", "CUBE"),
        }
        for kind, expected in cases.items():
            with self.subTest(kind=kind):
                detection = find_largest_detection(
                    make_synthetic_frame(kind), self.background
                )
                self.assertIsNotNone(detection)
                assert detection is not None
                self.assertEqual((detection.color, detection.shape), expected)
                self.assertGreater(detection.area, 900.0)

    def test_dimension_mismatch_fails_closed(self) -> None:
        frame = make_synthetic_frame("red_cube")
        with self.assertRaises(ValueError):
            find_largest_detection(frame[:200], self.background)

    def test_label_stabilizer_uses_majority(self) -> None:
        red = find_largest_detection(make_synthetic_frame("red_cube"), self.background)
        blue = find_largest_detection(
            make_synthetic_frame("blue_cylinder"), self.background
        )
        stabilizer = LabelStabilizer(window=5)
        stabilizer.update(red)
        stabilizer.update(red)
        stable = stabilizer.update(blue)
        assert red is not None
        self.assertEqual(stable, red.label)

    def test_overlay_changes_pixels_without_mutating_input(self) -> None:
        frame = make_synthetic_frame("red_cube")
        original = frame.copy()
        detection = find_largest_detection(frame, self.background)
        assert detection is not None
        rendered = render_overlay(frame, detection, detection.label, "SYNTHETIC", 30.0, True)
        self.assertTrue(np.array_equal(frame, original))
        self.assertFalse(np.array_equal(rendered, original))


if __name__ == "__main__":
    unittest.main()
