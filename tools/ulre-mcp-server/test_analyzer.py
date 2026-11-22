import json
import math
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import analyzer
import capture_manifest
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

    def manifest(self, name="capture.json", **overrides):
        metadata = {
            "schema": "ulre.capture.v1", "request_id": 1, "target_id": 2, "frame_id": 3,
            "source": "swapchain", "attachment": "color", "data_file": "capture.raw",
            "width": 1, "height": 1, "encoding": "bgra8", "color_space": "unknown",
            "vk_format": 44,
            "byte_order": "little", "samples": 1, "mip": 0, "layer": 0,
            "pixel_size": 4, "row_pitch": 4, "byte_count": 4,
        }
        metadata.update(overrides)
        path = self.root / name
        path.write_text(json.dumps(metadata), encoding="utf-8")
        return str(path)

    def test_bgra_luminance_uses_red_channel(self):
        path = self.capture("red.raw", bytes((0, 0, 255, 255)))
        result = analyzer.analyze_luminance(path, 1, 1, "bgra8")
        self.assertAlmostEqual(result["mean"], 0.2126)

    def test_srgb_luminance_is_linearized(self):
        path = self.capture("gray.raw", bytes((128, 128, 128, 255)))
        result = analyzer.analyze_luminance(path, 1, 1, "rgba8", "srgb")
        self.assertAlmostEqual(result["mean"], 0.2158605001)

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
                "color_space": "linear",
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

    def test_pipe_client_reads_fragmented_response(self):
        class Pipe:
            def __init__(self):
                self.chunks = iter([b'{"jsonrpc":"2.0","id":', b'1,"result":{"valid":true}}'])
            def __enter__(self):
                return self
            def __exit__(self, *_):
                pass
            def write(self, data):
                return len(data)
            def read(self, _):
                return next(self.chunks, b"")

        with patch.object(mcp_server.os, "name", "nt"), \
             patch("builtins.open", return_value=Pipe()):
            result = mcp_server.call_engine("get_debug_view")
        self.assertTrue(result["valid"])

    def test_pipe_client_rejects_mismatched_response_id(self):
        class Pipe:
            def __enter__(self):
                return self
            def __exit__(self, *_):
                pass
            def write(self, data):
                return len(data)
            def read(self, _):
                return b'{"jsonrpc":"2.0","id":2,"result":{"valid":true}}'

        with patch.object(mcp_server.os, "name", "nt"), \
             patch("builtins.open", return_value=Pipe()):
            result = mcp_server.call_engine("get_debug_view")
        self.assertFalse(result["valid"])

    def test_pipe_open_retries_busy_before_sending(self):
        sentinel = object()
        with patch("builtins.open", side_effect=[OSError(22, "busy"), sentinel]) as opened, \
             patch.object(mcp_server.time, "sleep"):
            self.assertIs(mcp_server.open_engine_pipe(), sentinel)
        self.assertEqual(opened.call_count, 2)

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

    def test_world_position_respects_viewport_and_depth_range(self):
        depth = self.capture("viewport.raw", struct.pack(
            "<8f", 0.0, 0.5, 0.5, 0.0, 0.0, 0.5, 0.5, 0.0))
        inverse = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
        viewport = {"x": 1, "y": 0, "width": 2, "height": 2,
                    "min_depth": 0.2, "max_depth": 0.8}
        result = analyzer.reconstruct_world_pos(depth, 4, 2, inverse, True, 1, 0, viewport)
        self.assertEqual(result["point_count"], 4)
        self.assertEqual(result["bounds"]["min"][:2], [-0.5, -0.5])
        self.assertEqual(result["bounds"]["max"][:2], [0.5, 0.5])
        self.assertAlmostEqual(result["bounds"]["min"][2], 0.5)

    def test_world_position_applies_column_major_translation(self):
        depth = self.capture("translated.raw", struct.pack("<f", 0.5))
        inverse = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0, 3.0, -2.0, 7.0, 1.0]
        result = analyzer.reconstruct_world_pos(depth, 1, 1, inverse, sample_step=1)
        self.assertEqual(result["bounds"]["min"], [3.0, -2.0, 7.5])
        self.assertEqual(result["bounds"]["max"], [3.0, -2.0, 7.5])

    def test_invalid_depth_is_not_reported_as_valid(self):
        depth = self.capture("invalid.raw", struct.pack("<3f", math.nan, math.inf, -1.0))
        result = analyzer.analyze_depth(depth, 3, 1)
        self.assertFalse(result["valid"])
        self.assertEqual(result["error"], "no_valid_depth_samples")
        self.assertIsNone(result["effective_depth_ratio"])
        self.assertEqual((result["nan_count"], result["inf_count"],
                          result["out_of_range_count"]), (1, 1, 1))

    def test_world_position_with_only_clear_pixels_has_no_result(self):
        depth = self.capture("clear.raw", struct.pack("<f", 0.0))
        inverse = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
        result = analyzer.reconstruct_world_pos(depth, 1, 1, inverse, True, 1)
        self.assertFalse(result["valid"])
        self.assertEqual(result["error"], "no_valid_non_clear_samples")

    def test_manifest_world_position_requires_known_coordinate_space(self):
        self.capture("capture.raw", struct.pack("<f", 0.5))
        camera = {"reversed_z": True, "matrix_layout": "column_major",
                  "position_space": "unspecified", "inverse_vp": [1.0] * 16}
        path = self.manifest(attachment="depth", encoding="r32f", vk_format=130,
                             copy_aspect="depth", clear_depth=0.0,
                             camera_status="recorded", camera=camera)
        with self.assertRaisesRegex(ValueError, "absolute-world"):
            capture_manifest.analyze_capture(path, "world_position")
        camera["position_space"] = "world_absolute"
        path = self.manifest(attachment="depth", encoding="r32f", vk_format=130,
                             copy_aspect="depth", clear_depth=0.0,
                             camera_status="recorded", camera=camera)
        with self.assertRaisesRegex(ValueError, "explicit opt-in"):
            capture_manifest.analyze_capture(path, "world_position")

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

    def test_manifest_luminance_requires_known_color_space(self):
        self.capture("capture.raw", bytes((0, 0, 255, 255)))
        path = self.manifest()
        with self.assertRaisesRegex(ValueError, "color space is unknown"):
            capture_manifest.analyze_capture(path, "luminance")
        result = capture_manifest.analyze_capture(path, "luminance", "linear")
        self.assertAlmostEqual(result["mean"], 0.2126)
        self.assertEqual(result["capture"]["frame_id"], 3)

    def test_manifest_depth_uses_recorded_clear_value(self):
        self.capture("capture.raw", struct.pack("<ff", 0.0, 1.0))
        path = self.manifest(attachment="depth", encoding="r32f", vk_format=130, width=2,
                             row_pitch=8, byte_count=8, copy_aspect="depth",
                             clear_depth=0.0, camera_status="recorded",
                             camera={"reversed_z": False})
        result = capture_manifest.analyze_capture(path, "depth")
        self.assertEqual(result["effective_depth_ratio"], 0.5)
        self.assertEqual(result["clear_depth"], 0.0)
        with self.assertRaisesRegex(ValueError, "conflicts"):
            capture_manifest.analyze_capture(path, "depth", reversed_z_override=True)

    def test_manifest_analysis_respects_row_padding(self):
        self.capture("capture.raw", bytes((0, 0, 255, 255, 123, 123,
                                            255, 0, 0, 255, 123, 123)))
        path = self.manifest(height=2, row_pitch=6, byte_count=12)
        color = capture_manifest.analyze_capture(path, "luminance", "linear")
        self.assertAlmostEqual(color["mean"], (0.2126 + 0.0722) / 2)

        self.capture("capture.raw", struct.pack("<f", 0.0) + b"PAD!" +
                     struct.pack("<f", 0.5) + b"PAD!")
        path = self.manifest(attachment="depth", encoding="r32f", vk_format=130,
                             height=2, row_pitch=8, byte_count=16, copy_aspect="depth",
                             clear_depth=0.0, camera_status="recorded",
                             camera={"reversed_z": True})
        depth = capture_manifest.analyze_capture(path, "depth")
        self.assertEqual(depth["effective_depth_ratio"], 0.5)

    def test_debug_view_mask_excludes_background(self):
        self.capture("capture.raw", bytes((128, 128, 255, 255, 0, 0, 0, 0)))
        path = self.manifest(source="debug_view", debug_view_mode="world_normal",
                             attachment="color", encoding="rgba8", color_space="linear",
                             vk_format=37, width=2, row_pitch=8, byte_count=8,
                             mask="alpha_nonzero")
        result = capture_manifest.analyze_capture(path, "normals")
        self.assertEqual(result["sample_count"], 1)
        self.assertEqual(result["pixel_scope"], "valid_geometry_mask")
        self.assertEqual(result["length_error_ratio"], 0.0)
        with self.assertRaisesRegex(ValueError, "matching masked"):
            capture_manifest.analyze_capture(path, "surface")
        self.capture("capture.raw", bytes(8))
        with self.assertRaisesRegex(ValueError, "no valid masked pixels"):
            capture_manifest.analyze_capture(path, "normals")

    def test_masked_frame_diff_reports_coverage_changes(self):
        a = self.capture("a.raw", bytes((0, 0, 0, 255, 0, 0, 0, 0)))
        b = self.capture("b.raw", bytes(8))
        result = analyzer.compare_frames(a, b, 2, 1, heatmap_dir=self.root, mask_alpha=True)
        self.assertEqual(result["changed_ratio"], 1.0)
        self.assertEqual(result["mask_changed_ratio"], 0.5)
        self.assertEqual(result["sample_count"], 1)
        with self.assertRaisesRegex(ValueError, "no valid masked"):
            analyzer.compare_frames(b, b, 2, 1, heatmap_dir=self.root, mask_alpha=True)

    def test_debug_mask_rejects_non_binary_alpha(self):
        image = self.capture("invalid-mask.raw", bytes((128, 128, 255, 127)))
        with self.assertRaisesRegex(ValueError, "non-binary alpha"):
            analyzer.analyze_normals(image, 1, 1, mask_alpha=True)
        with self.assertRaisesRegex(ValueError, "non-binary alpha"):
            analyzer.compare_frames(image, image, 1, 1, heatmap_dir=self.root, mask_alpha=True)

    def test_depth_reconstruction_requires_matching_same_frame_mask(self):
        self.capture("color.raw", bytes((128, 128, 255, 255, 0, 0, 0, 0)))
        self.capture("depth.raw", struct.pack("<ff", 0.0, 0.5))
        inverse = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                   0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]
        shared = {"source": "debug_view", "debug_view_mode": "world_normal",
                  "width": 2, "height": 1, "row_pitch": 8, "byte_count": 8,
                  "capture_group_id": "color.raw", "camera_status": "recorded",
                  "camera": {"matrix_layout": "column_major", "position_space": "world_absolute",
                             "inverse_vp": inverse, "reversed_z": True},
                  "viewport": {"x": 0, "y": 0, "width": 2, "height": 1,
                               "min_depth": 0, "max_depth": 1}}
        color = self.manifest("color.json", data_file="color.raw", capture_id="color.raw",
                              attachment="color", encoding="rgba8", vk_format=37,
                              color_space="linear", mask="alpha_nonzero", **shared)
        depth = self.manifest("depth.json", data_file="depth.raw", attachment="depth",
                              encoding="r32f", vk_format=126, color_space="unknown",
                              mask="unavailable", copy_aspect="depth", clear_depth=0.0, **shared)
        result = capture_manifest.analyze_capture(depth, "world_position", sample_step=1,
                                                  mask_metadata_path=color)
        self.assertTrue(result["valid"])
        self.assertEqual(result["point_count"], 1)
        self.assertEqual(result["bounds"]["min"], [-0.5, 0.0, 0.0])
        self.assertEqual(result["pixel_scope"], "sampled_valid_geometry_mask")
        altered = dict(shared)
        altered["frame_id"] = 4
        different = self.manifest("different.json", data_file="color.raw", capture_id="color.raw",
                                  attachment="color", encoding="rgba8", vk_format=37,
                                  color_space="linear", mask="alpha_nonzero", **altered)
        with self.assertRaisesRegex(ValueError, "frame_id differ"):
            capture_manifest.analyze_capture(depth, "world_position", sample_step=1,
                                             mask_metadata_path=different)
        altered = dict(shared)
        altered["capture_group_id"] = "other.raw"
        different = self.manifest("different.json", data_file="color.raw", capture_id="color.raw",
                                  attachment="color", encoding="rgba8", vk_format=37,
                                  color_space="linear", mask="alpha_nonzero", **altered)
        with self.assertRaisesRegex(ValueError, "same capture group"):
            capture_manifest.analyze_capture(depth, "world_position", sample_step=1,
                                             mask_metadata_path=different)
        self.capture("color.raw", bytes((128, 128, 255, 127, 0, 0, 0, 0)))
        with self.assertRaisesRegex(ValueError, "non-binary alpha"):
            capture_manifest.analyze_capture(depth, "world_position", sample_step=1,
                                             mask_metadata_path=color)

    def test_analysis_rejects_oversized_inputs(self):
        with self.assertRaisesRegex(ValueError, "pixel limit"):
            analyzer._float32_channels(bytes(4), analyzer.MAX_ANALYSIS_PIXELS + 1, 1, "rgba8")
        path = self.capture("oversized.raw", bytes(4))
        with patch.object(analyzer, "MAX_ANALYSIS_BYTES", 2):
            with self.assertRaisesRegex(ValueError, "byte limit"):
                analyzer.analyze_luminance(path, 1, 1)

    def test_manifest_rejects_bad_layout_and_traversal(self):
        self.capture("capture.raw", bytes(4))
        for fields in ({"row_pitch": 8, "byte_count": 8},
                       {"data_file": "../capture.raw"},
                       {"data_file": "C:outside.raw"},
                       {"vk_format": 37},
                       {"capture_id": "another-capture.raw"},
                       {"samples": True}):
            with self.subTest(fields=fields):
                with self.assertRaises(ValueError):
                    capture_manifest.load_manifest(self.manifest(**fields))

    def test_compare_manifests_checks_context_and_channel_error(self):
        self.capture("a.raw", bytes((0, 0, 0, 255)))
        self.capture("b.raw", bytes((0, 0, 255, 255, 9, 9, 9, 9)))
        camera = {"matrix_layout": "column_major",
                  "vp": [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]}
        viewport = {"x": 0, "y": 0, "width": 1, "height": 1,
                    "min_depth": 0, "max_depth": 1}
        shared = {"camera_status": "recorded", "camera": camera, "viewport": viewport}
        a = self.manifest("a.json", data_file="a.raw", **shared)
        b = self.manifest("b.json", data_file="b.raw", frame_id=4, row_pitch=8,
                          byte_count=8, **shared)
        with self.assertRaisesRegex(ValueError, "explicit opt-in"):
            capture_manifest.compare_captures(a, b, heatmap_dir=self.root,
                                              color_space_override="linear")
        result = capture_manifest.compare_captures(a, b, heatmap_dir=self.root,
                                                   color_space_override="linear",
                                                   allow_unverified_context=True)
        self.assertAlmostEqual(result["mae"], 1 / 3)
        self.assertAlmostEqual(result["rmse"], math.sqrt(1 / 3))
        self.assertEqual(result["comparison_space"], "encoded_channels")
        changed_camera = dict(camera, vp=[2.0] + camera["vp"][1:])
        b = self.manifest("b.json", data_file="b.raw", frame_id=4, row_pitch=8, byte_count=8,
                          camera_status="recorded", camera=changed_camera, viewport=viewport)
        with self.assertRaisesRegex(ValueError, "cameras differ"):
            capture_manifest.compare_captures(a, b, color_space_override="linear",
                                              allow_unverified_context=True)


if __name__ == "__main__":
    unittest.main()
