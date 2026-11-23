import json
import struct
import tempfile
import time
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

    def test_audit_forwards_snapshot_selectors(self):
        original = mcp_server.call_engine
        captured = {}

        def fake_call(method, params=None):
            captured["method"] = method
            captured["params"] = params
            return {"valid": False, "error": "pipeline_snapshot_not_found"}

        mcp_server.call_engine = fake_call
        try:
            mcp_server.handle({
                "id": 3,
                "method": "tools/call",
                "params": {
                    "name": "audit_cache_invariants",
                    "arguments": {"world_id": 17, "frame_serial": 23},
                },
            })
        finally:
            mcp_server.call_engine = original

        self.assertEqual(captured["method"], "inspect_render_pipeline")
        self.assertEqual(captured["params"], {"world_id": 17, "frame_serial": 23})

    def test_audit_reports_unknown_for_evicted_snapshot(self):
        original = mcp_server.call_engine
        mcp_server.call_engine = lambda method, params=None: {
            "valid": False, "error": "pipeline_snapshot_not_found"}
        try:
            response = mcp_server.handle({
                "id": 6,
                "method": "tools/call",
                "params": {
                    "name": "audit_cache_invariants",
                    "arguments": {"world_id": 1, "frame_serial": 99},
                },
            })
        finally:
            mcp_server.call_engine = original

        result = json.loads(response["result"]["content"][0]["text"])
        self.assertTrue(result["valid"])
        self.assertEqual(result["status"], "UNKNOWN")
        self.assertEqual(result["reason"], "pipeline_snapshot_not_found")

    def test_audit_reports_unknown_for_replayed_collect(self):
        snapshot = {
            "valid": True,
            "schema_version": 2,
            "world_id": 1,
            "frame_serial": 9,
            "coverage": {"collect": {"available": True, "complete": False,
                                     "reason": "collect_fast_path_replay"}},
            "funnel": {"passed_can_render": 10, "collect_candidates": 10,
                       "collected_items": 10, "gpu_draw_count_available": False},
            "cache_state": {
                "fast_path_engaged": True, "last_collect_steady": False,
                "has_pending_resources": False, "snapshot_ready": True,
                "snapshot_topology_rev": 4, "world_topology_rev": 4,
            },
            "drops": {},
        }
        original = mcp_server.call_engine
        mcp_server.call_engine = lambda method, params=None: snapshot
        try:
            response = mcp_server.handle({
                "id": 4,
                "method": "tools/call",
                "params": {"name": "audit_cache_invariants", "arguments": {}},
            })
        finally:
            mcp_server.call_engine = original

        result = json.loads(response["result"]["content"][0]["text"])
        self.assertEqual(result["status"], "UNKNOWN")
        collect_rule = next(rule for rule in result["rules"]
                            if rule["rule"] == "CollectFunnelMonotonicity")
        self.assertEqual(collect_rule["status"], "UNKNOWN")
        self.assertEqual(collect_rule["reason"], "collect_fast_path_replay")

    def test_audit_fails_non_monotonic_collect_counts(self):
        snapshot = {
            "valid": True,
            "schema_version": 2,
            "world_id": 1,
            "frame_serial": 10,
            "coverage": {"collect": {"available": True, "complete": True, "reason": None}},
            "funnel": {"passed_can_render": 5, "collect_candidates": 7,
                       "collected_items": 8, "gpu_draw_count_available": False},
            "cache_state": {
                "fast_path_engaged": False, "last_collect_steady": False,
                "has_pending_resources": False, "snapshot_ready": True,
                "snapshot_topology_rev": 4, "world_topology_rev": 4,
            },
            "drops": {},
        }
        original = mcp_server.call_engine
        mcp_server.call_engine = lambda method, params=None: snapshot
        try:
            response = mcp_server.handle({
                "id": 5,
                "method": "tools/call",
                "params": {"name": "audit_cache_invariants", "arguments": {}},
            })
        finally:
            mcp_server.call_engine = original

        result = json.loads(response["result"]["content"][0]["text"])
        self.assertEqual(result["status"], "FAIL")
        collect_rule = next(rule for rule in result["rules"]
                            if rule["rule"] == "CollectFunnelMonotonicity")
        self.assertEqual(collect_rule["status"], "FAIL")

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

    def test_engine_pipe_call_has_bounded_timeout(self):
        original = mcp_server._call_engine_once

        def slow_call(method, params=None):
            time.sleep(4.0)
            return {"valid": True}

        mcp_server._call_engine_once = slow_call
        started = time.monotonic()
        try:
            result = mcp_server.call_engine("get_render_health")
        finally:
            mcp_server._call_engine_once = original

        self.assertFalse(result["valid"])
        self.assertEqual(result["error"], "ULRE bridge call timeout")
        self.assertLess(time.monotonic() - started, 3.5)


if __name__ == "__main__":
    unittest.main()
