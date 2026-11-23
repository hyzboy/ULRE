import json
import struct
import tempfile
import unittest
from pathlib import Path

import analyzer
import capture_manifest
import mcp_server


class MCPReviewFixTests(unittest.TestCase):
    def test_deprecated_dump_tools_do_not_call_engine(self):
        for tool_name in ("dump_buffer", "dump_render_target"):
            response = mcp_server.handle({
                "id": 1,
                "method": "tools/call",
                "params": {"name": tool_name, "arguments": {"name": "ignored"}},
            })
            text = response["result"]["content"][0]["text"]
            self.assertIn('"error": "deprecated_dump_api"', text)
            self.assertIn('"valid": false', text)

    def test_audit_missing_fields_is_unknown_not_pass(self):
        original = mcp_server.call_engine
        mcp_server.call_engine = lambda method, params=None: {
            "valid": True,
            "cache_state": {},
            "funnel": {},
            "drops": {},
        }
        try:
            response = mcp_server.handle({
                "id": 2,
                "method": "tools/call",
                "params": {"name": "audit_cache_invariants", "arguments": {}},
            })
        finally:
            mcp_server.call_engine = original

        text = response["result"]["content"][0]["text"]
        self.assertIn('"status": "UNKNOWN"', text)
        self.assertIn('"missing_fields"', text)
        self.assertNotIn('"status": "PASS"', text)

    def test_d16_and_d24_depth_decoding(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            d16 = root / "d16.raw"
            d16.write_bytes(struct.pack("<H", 32768))
            d16_result = analyzer.analyze_depth(str(d16), 1, 1, False, 1.0,
                                                2, None, "d16_unorm")
            self.assertAlmostEqual(d16_result["mean"], 32768 / 65535.0)

            d24 = root / "d24.raw"
            d24.write_bytes(struct.pack("<I", 8388607))
            d24_result = analyzer.analyze_depth(str(d24), 1, 1, False, 1.0,
                                                4, None, "d24_unorm_32")
            self.assertAlmostEqual(d24_result["mean"], 8388607 / 16777215.0)

    def test_offscreen_pixel_only_comparison_requires_explicit_opt_in(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifests = []
            for label, pixel in (("a", b"\x00\x00\x00\xff"), ("b", b"\xff\xff\xff\xff")):
                data = root / (label + ".raw")
                data.write_bytes(pixel)
                manifest = root / (label + ".json")
                manifest.write_text(json.dumps({
                    "schema": "ulre.capture.v1",
                    "request_id": 1,
                    "target_id": 2,
                    "frame_id": 3,
                    "source": "offscreen",
                    "debug_view_mode": "off",
                    "attachment": "color",
                    "capture_id": data.name,
                    "data_file": data.name,
                    "width": 1,
                    "height": 1,
                    "vk_format": 43,
                    "encoding": "rgba8",
                    "color_space": "srgb",
                    "byte_order": "little",
                    "samples": 1,
                    "mip": 0,
                    "layer": 0,
                    "pixel_size": 4,
                    "row_pitch": 4,
                    "byte_count": 4,
                    "mask": "unavailable",
                    "viewport": None,
                    "camera_status": "unavailable",
                    "camera": None,
                }), encoding="utf-8")
                manifests.append(manifest)

            with self.assertRaises(ValueError):
                capture_manifest.compare_captures(
                    str(manifests[0]), str(manifests[1]),
                    heatmap_dir=str(root),
                    allow_unverified_context=True)

            result = capture_manifest.compare_captures(
                str(manifests[0]), str(manifests[1]),
                heatmap_dir=str(root),
                allow_unverified_context=True,
                allow_pixel_only_unverified=True)
            self.assertEqual(result["context"]["comparison_kind"], "pixel_only")


if __name__ == "__main__":
    unittest.main()
