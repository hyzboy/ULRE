import json
import math
import os
import struct
import tempfile
import time
import unittest
from pathlib import Path

import mcp_server
import capture_manifest


@unittest.skipUnless(os.environ.get("ULRE_CAPTURE_TEST") == "1",
                     "requires a running engine with DebugBridge enabled")
class CaptureIntegrationTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("ULRE_OFFSCREEN_TEST") == "1",
                         "requires an ordinary offscreen target rendered on request")
    @unittest.skipIf(os.environ.get("ULRE_OFFSCREEN_UNSAFE_TEST") == "1",
                     "fixture is intentionally rendering an unsupported pass")
    def test_ordinary_offscreen_color_depth(self):
        target = None
        for _ in range(150):
            targets = mcp_server.call_engine("list_capture_targets")
            target = next((item for item in targets.get("targets", [])
                           if item["source"] == "offscreen"
                           and {"color", "depth"} <= set(item["attachments"])), None)
            if target:
                break
            time.sleep(0.1)
        self.assertIsNotNone(target, targets)
        details = target["attachment_details"]
        self.assertEqual(target["color_attachment_count"], 1)
        self.assertEqual([entry["attachment"] for entry in details], ["color", "depth"])
        self.assertTrue(all(entry["available"] and entry["vk_format"] > 0 for entry in details))
        previous_frame = 0
        for _ in range(2):
            request = mcp_server.call_engine("capture_frame", {
                "target_id": target["target_id"], "attachment": "color_depth",
            })
            self.assertTrue(request.get("valid"), request)
            try:
                for _ in range(150):
                    result = mcp_server.call_engine("get_capture_status", {
                        "request_id": request["request_id"],
                    })
                    if result.get("state") not in ("queued", "capturing"):
                        break
                    time.sleep(0.1)
                self.assertEqual(result.get("state"), "succeeded", result)
                self.assertGreater(result["frame_id"], previous_frame)
                previous_frame = result["frame_id"]
                manifests = [json.loads(Path(entry["metadata_path"]).read_text(encoding="utf-8"))
                             for entry in result["captures"]]
                for entry, manifest in zip(result["captures"], manifests):
                    self.assertEqual(manifest["source"], "offscreen")
                    self.assertEqual(manifest["target_id"], target["target_id"])
                    self.assertEqual(manifest["frame_id"], result["frame_id"])
                    self.assertEqual(manifest["camera_status"], "unavailable")
                    self.assertIsNone(manifest["viewport"])
                    self.assertEqual(Path(entry["dump_path"]).stat().st_size, manifest["byte_count"])
                self.assertEqual(manifests[1]["clear_depth"], 0)
                self.assertEqual(manifests[0]["capture_group_id"], manifests[1]["capture_group_id"])
                color = Path(result["captures"][0]["dump_path"]).read_bytes()
                self.assertGreater(len({color[i:i + 4] for i in range(0, len(color), 4)}), 1)
                self.assertEqual(manifests[1]["encoding"], "r32f")
                depth = Path(result["captures"][1]["dump_path"]).read_bytes()
                depths = [value[0] for value in struct.iter_unpack("<f", depth)]
                self.assertTrue(all(math.isfinite(value) and 0 <= value <= 1 for value in depths))
                self.assertTrue(any(value > 0 for value in depths))
                self.assertTrue(any(value == 0 for value in depths))
            finally:
                mcp_server.call_engine("release_capture", {"request_id": request["request_id"]})

    @unittest.skipUnless(os.environ.get("ULRE_OFFSCREEN_UNSAFE_TEST") == "1",
                         "requires the unsupported-pass fixture")
    def test_ordinary_offscreen_rejects_scissor_pass(self):
        for _ in range(150):
            targets = mcp_server.call_engine("list_capture_targets")
            target = next((item for item in targets.get("targets", [])
                           if item["source"] == "offscreen" and "color" in item["attachments"]), None)
            if target:
                break
            time.sleep(0.1)
        self.assertIsNotNone(target, targets)
        request = mcp_server.call_engine("capture_frame", {
            "target_id": target["target_id"], "attachment": "color",
        })
        self.assertTrue(request.get("valid"), request)
        try:
            for _ in range(150):
                result = mcp_server.call_engine("get_capture_status", {
                    "request_id": request["request_id"],
                })
                if result.get("state") not in ("queued", "capturing"):
                    break
                time.sleep(0.1)
            self.assertEqual(result.get("state"), "failed", result)
            self.assertEqual(result.get("error"), "frame_unavailable")
            self.assertFalse(result.get("dump_path"))
        finally:
            mcp_server.call_engine("release_capture", {"request_id": request["request_id"]})

    @unittest.skipUnless(os.environ.get("ULRE_DEBUG_VIEW_TEST") == "1",
                         "requires the static BasicLitMeshes fixture")
    def test_isolated_debug_modes_leave_main_frame_unchanged(self):
        def capture(target, attachment="color"):
            request = mcp_server.call_engine("capture_frame", {
                "target_id": target, "attachment": attachment,
            })
            self.assertTrue(request.get("valid"), request)
            for _ in range(150):
                result = mcp_server.call_engine("get_capture_status", {
                    "request_id": request["request_id"],
                })
                if result.get("state") not in ("queued", "capturing"):
                    break
                time.sleep(0.1)
            self.assertEqual(result.get("state"), "succeeded", result)
            self.assertTrue(mcp_server.call_engine("release_capture", {
                "request_id": request["request_id"],
            }).get("valid"))
            return result

        targets = mcp_server.call_engine("list_capture_targets")["targets"]
        main_target = next(item["target_id"] for item in targets if item["kind"] == "swapchain")
        before = capture(main_target)
        try:
            for mode in ("world_normal", "base_color", "surface", "emissive", "uv0"):
                with self.subTest(mode=mode):
                    self.assertTrue(mcp_server.call_engine("set_debug_view", {"mode": mode})["valid"])
                    for _ in range(300):
                        status = mcp_server.call_engine("get_debug_view")
                        if status.get("mode") == mode or status.get("error"):
                            break
                        time.sleep(0.1)
                    self.assertEqual(status.get("mode"), mode, status)
                    self.assertGreater(status["tracked_entities"], 0)
                    captured = capture(status["target_id"])
                    manifest = json.loads(Path(captured["metadata_path"]).read_text(encoding="utf-8"))
                    self.assertEqual(manifest["source"], "debug_view")
                    self.assertEqual(manifest["debug_view_mode"], mode)
                    self.assertEqual(manifest["mask"], "alpha_nonzero")
                    if mode in ("world_normal", "surface"):
                        operation = "normals" if mode == "world_normal" else "surface"
                        analysis = capture_manifest.analyze_capture(captured["metadata_path"], operation)
                        self.assertGreater(analysis["sample_count"], 0)
                        if mode == "world_normal":
                            self.assertLess(analysis["length_error_ratio"], 0.01)
                            pair = capture(status["target_id"], "color_depth")
                            points = capture_manifest.analyze_capture(
                                pair["captures"][1]["metadata_path"], "world_position", sample_step=8,
                                mask_metadata_path=pair["captures"][0]["metadata_path"])
                            self.assertTrue(points["valid"], points)
                            self.assertGreater(points["point_count"], 0)
                            for value in points["bounds"]["min"] + points["bounds"]["max"]:
                                self.assertLess(abs(value), 30.0, points)
            after = capture(main_target)
            self.assertEqual(Path(before["dump_path"]).read_bytes(),
                             Path(after["dump_path"]).read_bytes())
        finally:
            mcp_server.call_engine("set_debug_view", {"mode": "off"})
            for _ in range(150):
                status = mcp_server.call_engine("get_debug_view")
                if status.get("mode") == "off" and not status.get("target_id"):
                    break
                time.sleep(0.1)
            self.assertEqual(status.get("mode"), "off", status)
            self.assertFalse(status.get("target_id"), status)

        for _ in range(8):
            mcp_server.call_engine("set_debug_view", {"mode": "world_normal"})
            for _ in range(300):
                status = mcp_server.call_engine("get_debug_view")
                if status.get("mode") == "world_normal" or status.get("error"):
                    break
                time.sleep(0.1)
            self.assertEqual(status.get("mode"), "world_normal", status)
            old_target = status["target_id"]
            mcp_server.call_engine("set_debug_view", {"mode": "off"})
            for _ in range(150):
                status = mcp_server.call_engine("get_debug_view")
                if status.get("mode") == "off" and not status.get("target_id"):
                    break
                time.sleep(0.1)
            self.assertEqual(status.get("mode"), "off", status)
            self.assertNotIn(old_target, [item["target_id"] for item in
                                          mcp_server.call_engine("list_capture_targets")["targets"]])

    def test_color_and_depth_share_a_submitted_frame(self):
        import time
        targets = mcp_server.call_engine("list_capture_targets")
        target = next(item for item in targets["targets"] if "color_depth" in item["attachments"])
        request = mcp_server.call_engine("capture_frame", {
            "target_id": target["target_id"], "attachment": "color_depth",
        })
        self.assertTrue(request.get("valid"), request)
        request_id = request["request_id"]
        for _ in range(150):
            result = mcp_server.call_engine("get_capture_status", {"request_id": request_id})
            if result.get("state") not in ("queued", "capturing"):
                break
            time.sleep(0.1)
        try:
            self.assertEqual(result.get("state"), "succeeded", result)
            captures = result["captures"]
            self.assertEqual([entry["attachment"] for entry in captures], ["color", "depth"])
            manifests = [json.loads(Path(entry["metadata_path"]).read_text(encoding="utf-8"))
                         for entry in captures]
            self.assertNotEqual(manifests[0]["capture_id"], manifests[1]["capture_id"])
            self.assertEqual(manifests[0]["capture_group_id"], manifests[0]["capture_id"])
            self.assertEqual(manifests[1]["capture_group_id"], manifests[0]["capture_id"])
            self.assertEqual(manifests[0]["request_id"], request_id)
            self.assertEqual(manifests[1]["request_id"], request_id)
            self.assertEqual(manifests[0]["frame_id"], result["frame_id"])
            self.assertEqual(manifests[1]["frame_id"], result["frame_id"])
            self.assertEqual(manifests[0]["camera"], manifests[1]["camera"])
            self.assertEqual(manifests[0]["viewport"], manifests[1]["viewport"])
            for entry, manifest in zip(captures, manifests):
                self.assertEqual(Path(entry["dump_path"]).stat().st_size, manifest["byte_count"])
        finally:
            released = mcp_server.call_engine("release_capture", {"request_id": request_id})
            self.assertTrue(released.get("valid"), released)

    def test_swapchain_captures_have_consistent_manifests(self):
        targets = mcp_server.call_engine("list_capture_targets")
        self.assertTrue(targets.get("valid"), targets)
        target = next(item for item in targets["targets"]
                      if {"color", "depth"} <= set(item["attachments"]))
        for attachment in ("color", "depth"):
            with self.subTest(attachment=attachment):
                request = mcp_server.call_engine("capture_frame", {
                    "target_id": target["target_id"], "attachment": attachment,
                })
                self.assertTrue(request.get("valid"), request)
                request_id = request["request_id"]
                import time
                for _ in range(150):
                    result = mcp_server.call_engine("get_capture_status", {
                        "request_id": request_id,
                    })
                    if result.get("state") not in ("queued", "capturing"):
                        break
                    time.sleep(0.1)
                try:
                    self.assertEqual(result.get("state"), "succeeded", result)
                    metadata_path = Path(result["metadata_path"])
                    self.assertTrue(metadata_path.is_absolute())
                    manifest = json.loads(metadata_path.read_text(encoding="utf-8"))
                    self.assertEqual(manifest["schema"], "ulre.capture.v1")
                    self.assertEqual(manifest["request_id"], request_id)
                    self.assertEqual(manifest["frame_id"], result["frame_id"])
                    self.assertEqual(manifest["attachment"], attachment)
                    data_path = metadata_path.parent / manifest["data_file"]
                    self.assertEqual(data_path, Path(result["dump_path"]))
                    self.assertEqual(data_path.stat().st_size, manifest["byte_count"])
                    self.assertEqual(manifest["byte_count"],
                                     manifest["row_pitch"] * manifest["height"])
                    self.assertEqual(manifest["camera_status"], "recorded")
                    camera = manifest["camera"]
                    self.assertEqual(camera["matrix_layout"], "column_major")
                    vp, inverse = camera["vp"], camera["inverse_vp"]
                    for row in range(4):
                        for col in range(4):
                            value = sum(vp[k * 4 + row] * inverse[col * 4 + k]
                                        for k in range(4))
                            self.assertAlmostEqual(value, float(row == col), delta=0.001)
                    self.assertGreater(camera["znear"], 0)
                    operation = "luminance" if attachment == "color" else "depth"
                    arguments = {"metadata_path": str(metadata_path), "operation": operation}
                    if attachment == "color" and manifest["color_space"] == "unknown":
                        arguments["color_space_override"] = "linear"
                    analysis = mcp_server.handle({
                        "id": 11, "method": "tools/call",
                        "params": {"name": "analyze_capture", "arguments": arguments},
                    })
                    self.assertFalse(analysis["result"]["isError"], analysis)
                    values = json.loads(analysis["result"]["content"][0]["text"])
                    self.assertEqual(values["capture"]["frame_id"], manifest["frame_id"])
                    if attachment == "color":
                        with tempfile.TemporaryDirectory() as output:
                            comparison = mcp_server.handle({
                                "id": 13, "method": "tools/call",
                                "params": {"name": "compare_captures", "arguments": {
                                    "metadata_path_a": str(metadata_path),
                                    "metadata_path_b": str(metadata_path),
                                    "color_space_override": manifest["color_space"]
                                        if manifest["color_space"] != "unknown" else "linear",
                                    "allow_unverified_context": True,
                                    "heatmap_dir": output,
                                }},
                            })
                            self.assertFalse(comparison["result"]["isError"], comparison)
                            diff = json.loads(comparison["result"]["content"][0]["text"])
                            self.assertEqual(diff["rmse"], 0)
                            self.assertTrue(diff["psnr_infinite"])
                    if attachment == "depth":
                        arguments["operation"] = "world_position"
                        arguments["sample_step"] = 8
                        arguments["allow_unverified_depth_producers"] = True
                        analysis = mcp_server.handle({
                            "id": 12, "method": "tools/call",
                            "params": {"name": "analyze_capture", "arguments": arguments},
                        })
                        self.assertFalse(analysis["result"]["isError"], analysis)
                        points = json.loads(analysis["result"]["content"][0]["text"])
                        self.assertEqual(points["position_space"], "world_absolute")
                        self.assertGreater(points["point_count"], 0)
                finally:
                    released = mcp_server.call_engine("release_capture", {
                        "request_id": request_id,
                    })
                    self.assertTrue(released.get("valid"), released)
