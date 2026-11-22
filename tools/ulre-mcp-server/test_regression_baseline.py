import json
import struct
import tempfile
import unittest
from pathlib import Path

import regression_baseline as baseline


class BaselineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.scene = {"version": 1, "mode": "world_normal", "width": 2, "height": 1}
        self.path = self.root / "golden.json"

    def tearDown(self):
        self.tmp.cleanup()

    def capture(self, name, data, *, device_id=7, camera_offset=0):
        raw = self.root / (name + ".raw")
        raw.write_bytes(bytes(data))
        matrix = [float(i % 5 == 0) for i in range(16)]
        matrix[12] = camera_offset
        meta = {
            "schema": "ulre.capture.v1", "request_id": 1, "target_id": 1, "frame_id": 241,
            "source": "debug_view", "debug_view_mode": "world_normal", "attachment": "color",
            "data_file": raw.name, "capture_id": raw.name, "width": 2, "height": 1,
            "encoding": "rgba8", "color_space": "linear", "vk_format": 37,
            "byte_order": "little", "samples": 1, "mip": 0, "layer": 0,
            "pixel_size": 4, "row_pitch": 8, "byte_count": 8, "mask": "alpha_nonzero",
            "viewport": {"x": 0, "y": 0, "width": 2, "height": 1, "min_depth": 0, "max_depth": 1},
            "camera_status": "recorded", "camera": {
                "matrix_layout": "column_major", "vp": matrix, "reversed_z": True,
                "projection_convention": "vulkan_rh_zo", "world_up": "z_up",
                "position_space": "world_absolute", "viewport_y": "positive_height",
            },
            "device": {"vendor_id": 1, "device_id": device_id, "device_type": 1,
                       "api_version": 1, "driver_version": 1, "device_name": "test",
                       "driver_properties": {"driver_id": 1, "driver_name": "test", "driver_info": "1"}},
        }
        path = self.root / (name + ".json")
        path.write_text(json.dumps(meta), encoding="utf-8")
        return path

    def compare(self, capture, tolerance=None):
        return baseline.compare_baseline(self.path, capture, self.scene, self.root, tolerance)

    def test_explicit_creation_strict_comparison_and_update(self):
        a = self.capture("a", [128, 128, 255, 255, 0, 0, 0, 0])
        changed = self.capture("changed", [130, 128, 255, 255, 0, 0, 0, 0])
        with self.assertRaises(baseline.NotCovered):
            self.compare(a)
        baseline.write_baseline(self.path, a, self.scene)
        original = self.path.read_bytes()
        self.assertEqual(self.compare(a)["status"], "PASS")
        result = self.compare(changed)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(Path(result["comparison"]["heatmap_path"]).is_file())
        self.assertEqual(self.path.read_bytes(), original)
        with self.assertRaises(FileExistsError):
            baseline.write_baseline(self.path, changed, self.scene)
        baseline.write_baseline(self.path, changed, self.scene, update=True)
        self.assertEqual(self.compare(changed)["status"], "PASS")
        self.assertEqual(self.compare(a)["status"], "FAIL")

    def test_coverage_camera_device_and_scene_fail_closed(self):
        a = self.capture("a", [128, 128, 255, 255, 0, 0, 0, 0])
        coverage = self.capture("coverage", [128, 128, 255, 255, 10, 10, 10, 255])
        camera = self.capture("camera", [128, 128, 255, 255, 0, 0, 0, 0], camera_offset=1)
        other = self.capture("other", [129, 128, 255, 255, 0, 0, 0, 0], device_id=8)
        baseline.write_baseline(self.path, a, self.scene)
        self.assertEqual(self.compare(coverage)["status"], "FAIL")
        self.assertEqual(self.compare(camera)["status"], "FAIL")
        with self.assertRaises(baseline.NotCovered):
            self.compare(other)
        self.assertEqual(self.compare(other, 0.01)["status"], "PASS")
        self.assertEqual(self.compare(other, 0.001)["status"], "FAIL")
        for invalid in (float("nan"), -0.1, 1.1):
            with self.assertRaises(ValueError):
                self.compare(a, invalid)
        different = dict(self.scene, version=2)
        with self.assertRaises(baseline.NotCovered):
            baseline.compare_baseline(self.path, a, different, self.root)

    def test_missing_device_and_corrupt_baseline_rejected(self):
        a = self.capture("a", [128, 128, 255, 255, 0, 0, 0, 0])
        baseline.write_baseline(self.path, a, self.scene)
        descriptor = json.loads(self.path.read_text(encoding="utf-8"))
        saved = self.root / descriptor["capture_manifest"]
        saved.write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            self.compare(a)
        metadata = json.loads(a.read_text(encoding="utf-8"))
        metadata.pop("device")
        a.write_text(json.dumps(metadata), encoding="utf-8")
        with self.assertRaises(baseline.NotCovered):
            self.compare(a)

    def test_world_position_float_baseline(self):
        self.scene["mode"] = "world_position"

        def position(name, values, device_id=7):
            path = self.capture(name, struct.pack("<8f", *values), device_id=device_id)
            metadata = json.loads(path.read_text(encoding="utf-8"))
            metadata.update(debug_view_mode="world_position", encoding="rgba32f",
                            vk_format=109, pixel_size=16, row_pitch=32, byte_count=32,
                            position={"space": "world_absolute", "origin": [0, 0, 0],
                                      "coordinate_system": "right_handed_z_up",
                                      "representation": "ieee754_binary32", "format": "rgba32f",
                                      "mask": "alpha_nonzero"})
            path.write_text(json.dumps(metadata), encoding="utf-8")
            return path

        a = position("position-a", [1, 2, 3, 1, 0, 0, 0, 0])
        changed = position("position-changed", [1.01, 2, 3, 1, 0, 0, 0, 0])
        other = position("position-other", [1.01, 2, 3, 1, 0, 0, 0, 0], device_id=8)
        coverage = position("position-mask", [1, 2, 3, 1, 1, 2, 3, 1])
        baseline.write_baseline(self.path, a, self.scene)
        result = self.compare(a)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["comparison"]["algorithm"], "ulre_position_diff_v1")
        self.assertIsNone(result["comparison"]["ssim"])
        self.assertEqual(self.compare(changed)["status"], "FAIL")
        self.assertEqual(self.compare(coverage)["status"], "FAIL")
        self.assertEqual(self.compare(other, 0.02)["status"], "PASS")
        self.assertEqual(self.compare(other, 0.001)["status"], "FAIL")

    def test_shadow_caster_depth_and_coverage(self):
        self.scene["mode"] = "shadow_caster"

        def shadow(name, values, device_id=7):
            path = self.capture(name, struct.pack("<2f", *values), device_id=device_id)
            metadata = json.loads(path.read_text(encoding="utf-8"))
            metadata.update(source="offscreen", attachment="depth", encoding="r32f",
                            color_space="unknown", vk_format=126, copy_aspect="depth",
                            mask="unavailable", clear_depth=0.0, camera_status="unavailable",
                            camera=None, viewport=None)
            path.write_text(json.dumps(metadata), encoding="utf-8")
            return path

        a = shadow("shadow-a", [0.5, 0.0])
        removed = shadow("shadow-removed", [0.0, 0.0])
        drift = shadow("shadow-drift", [0.51, 0.0], device_id=8)
        invalid = shadow("shadow-invalid", [float("nan"), 0.0])
        baseline.write_baseline(self.path, a, self.scene)
        self.assertEqual(self.compare(a)["status"], "PASS")
        result = self.compare(removed)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["comparison"]["mask"]["coverage_changed_pixels"], 1)
        self.assertTrue(Path(result["comparison"]["heatmap_path"]).is_file())
        self.assertEqual(self.compare(drift, 0.02)["status"], "PASS")
        self.assertEqual(self.compare(drift, 0.001)["status"], "FAIL")
        self.assertEqual(self.compare(invalid)["status"], "FAIL")


if __name__ == "__main__":
    unittest.main()
