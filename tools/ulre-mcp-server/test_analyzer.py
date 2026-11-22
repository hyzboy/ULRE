import math
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import analyzer
import mcp_server


class AnalyzerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def capture(self, name, data):
        path = self.root / name
        path.write_bytes(data)
        return str(path)

    def test_bgra_luminance_uses_red_channel(self):
        path = self.capture("red.raw", bytes((0, 0, 255, 255)))
        result = analyzer.analyze_luminance(path, 1, 1, "bgra8")
        self.assertAlmostEqual(result["mean"], 0.2126)

    def test_unknown_format_is_rejected(self):
        path = self.capture("image.raw", bytes((0, 0, 0, 0)))
        with self.assertRaises(ValueError):
            analyzer.analyze_luminance(path, 1, 1, "unknown")

    def test_depth_clear_pixels_not_counted_as_coverage(self):
        path = self.capture("depth.raw", struct.pack("<ff", 0.0, 0.5))
        result = analyzer.analyze_depth(path, 2, 1, True)
        self.assertEqual(result["effective_depth_ratio"], 0.5)

    def test_frame_difference_uses_all_channels(self):
        a = self.capture("a.raw", bytes((0, 0, 0, 255)))
        b = self.capture("b.raw", bytes((255, 0, 0, 255)))
        result = analyzer.compare_frames(a, b, 1, 1, "rgba8", heatmap_dir=self.root)
        self.assertAlmostEqual(result["mae"], 1.0 / 3.0)
        self.assertAlmostEqual(result["rmse"], math.sqrt(1.0 / 3.0))

    def test_equal_frames_have_infinite_psnr_and_unique_heatmaps(self):
        image = self.capture("a.raw", bytes((42, 0, 0, 255)))
        first = analyzer.compare_frames(image, image, 1, 1, heatmap_dir=self.root)
        second = analyzer.compare_frames(image, image, 1, 1, heatmap_dir=self.root)
        self.assertIsNone(first["psnr"])
        self.assertTrue(first["psnr_infinite"])
        self.assertNotEqual(first["heatmap_path"], second["heatmap_path"])

    def test_packed_vk_formats_have_distinct_channel_order(self):
        low_red = self.capture("low.raw", struct.pack("<I", 1023))
        red = analyzer.analyze_luminance(low_red, 1, 1, "a2bgr10")
        blue = analyzer.analyze_luminance(low_red, 1, 1, "a2r10g10b10")
        self.assertAlmostEqual(red["mean"], 0.2126)
        self.assertAlmostEqual(blue["mean"], 0.0722)

    def test_capture_size_must_match_format(self):
        path = self.capture("extra.raw", bytes(8))
        with self.assertRaises(ValueError):
            analyzer.analyze_luminance(path, 1, 1)

    def test_standard_depth_clear_and_invalid(self):
        path = self.capture("depth.raw", struct.pack("<ffff", 1.0, 0.5, 2.0, math.nan))
        result = analyzer.analyze_depth(path, 4, 1)
        self.assertEqual(result["effective_depth_ratio"], 0.25)
        self.assertEqual(result["invalid_depth_ratio"], 0.5)

    def test_normals_do_not_claim_facing_without_view_direction(self):
        path = self.capture("normal.raw", bytes((128, 128, 255, 255)))
        result = analyzer.analyze_normals(path, 1, 1, "rgba8")
        self.assertNotIn("heuristic_front_facing_ratio", result)

    def test_mcp_marks_analysis_errors(self):
        response = mcp_server.handle({
            "id": 7,
            "method": "tools/call",
            "params": {"name": "analyze_luminance", "arguments": {
                "path": str(self.root / "missing.raw"), "width": 1, "height": 1,
            }},
        })
        self.assertTrue(response["result"]["isError"])

    def test_mcp_marks_unavailable_capture_as_error(self):
        request = {"id": 8, "method": "tools/call",
                   "params": {"name": "capture_frame", "arguments": {}}}
        with patch.object(mcp_server, "call_engine", return_value={
            "valid": False, "error": "capture_request_not_supported",
        }):
            response = mcp_server.handle(request)
        self.assertTrue(response["result"]["isError"])

    def test_world_position_does_not_read_latest_camera(self):
        depth = self.capture("depth.raw", struct.pack("<ff", 0.0, 0.5))
        inverse = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
        request = {"id": 9, "method": "tools/call",
                   "params": {"name": "reconstruct_world_pos", "arguments": {
                       "path": depth, "width": 2, "height": 1,
                       "inverse_vp": inverse, "reversed_z": True, "sample_step": 1,
                   }}}
        with patch.object(mcp_server, "call_engine", side_effect=AssertionError):
            response = mcp_server.handle(request)
        self.assertFalse(response["result"]["isError"])
        self.assertIn('"point_count": 1', response["result"]["content"][0]["text"])

    def test_missing_capture_matrix_is_an_error(self):
        request = {"id": 10, "method": "tools/call",
                   "params": {"name": "reconstruct_world_pos", "arguments": {
                       "path": "unused", "width": 1, "height": 1,
                   }}}
        with patch.object(mcp_server, "call_engine", side_effect=AssertionError):
            response = mcp_server.handle(request)
        self.assertTrue(response["result"]["isError"])

    def test_no_finite_luminance_is_an_error(self):
        path = self.capture("nan.raw", struct.pack("<f", math.nan))
        with self.assertRaises(ValueError):
            analyzer.analyze_luminance(path, 1, 1, "r32f")


if __name__ == "__main__":
    unittest.main()
