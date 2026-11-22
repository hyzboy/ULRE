import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

import render_regression


class FixtureAnalysisTests(unittest.TestCase):
    def test_normal_and_surface_assertions(self):
        normal = {"sample_count": 1, "nan_count": 0, "inf_count": 0,
                  "zero_normal_ratio": 0, "length_error_ratio": 0}
        render_regression._check_fixture_analysis(normal, "world_normal")
        with self.assertRaisesRegex(RuntimeError, "normal_length"):
            render_regression._check_fixture_analysis(
                dict(normal, zero_normal_ratio=0.1), "world_normal")
        with self.assertRaisesRegex(RuntimeError, "no_finite_samples"):
            render_regression._check_fixture_analysis(
                dict(normal, inf_count=1), "world_normal")
        surface = {"sample_count": 1, "metallic": {"mean": 0.08}}
        render_regression._check_fixture_analysis(surface, "surface")
        with self.assertRaisesRegex(RuntimeError, "metallic"):
            render_regression._check_fixture_analysis(
                dict(surface, metallic={"mean": 0.3}), "surface")

    def test_position_requires_crosscheck_samples(self):
        with self.assertRaisesRegex(RuntimeError, "no_samples"):
            render_regression._check_fixture_analysis(
                {"crosscheck": {"point_count": 0}}, "world_position")

    def test_known_normal_anchor_rejects_rotated_direction(self):
        metadata = {
            "debug_view_mode": "world_normal", "encoding": "rgba8",
            "width": 2, "height": 1, "row_pitch": 8,
            "viewport": {"x": 0, "y": 0, "width": 2, "height": 1},
            "camera": {"vp": [1, 0, 0, 0, 0, 1, 0, 0,
                               0, 0, 1, 0, 0, 0, 0, 1]},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "normal.raw"
            path.write_bytes(bytes((0, 0, 0, 0, 128, 128, 255, 255)))
            with patch.object(render_regression.capture_manifest, "load_manifest",
                              return_value=(metadata, path)):
                self.assertEqual(len(render_regression._check_known_normal_pixels("unused")), 2)
                path.write_bytes(bytes((0, 0, 0, 0, 255, 128, 128, 255)))
                with self.assertRaisesRegex(RuntimeError, "direction_mismatch"):
                    render_regression._check_known_normal_pixels("unused")


if __name__ == "__main__":
    unittest.main()
