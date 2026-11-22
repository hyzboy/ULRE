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
    def test_world_position_rt_matches_same_frame_depth_at_multiple_distances(self):
        requested = mcp_server.call_engine("set_debug_view", {"mode": "world_position"})
        self.assertTrue(requested.get("valid"), requested)
        try:
            for _ in range(300):
                status = mcp_server.call_engine("get_debug_view")
                if status.get("mode") == "world_position" or status.get("error"):
                    break
                time.sleep(0.1)
            if status.get("error") in ("position_format_unavailable",
                                      "position_target_creation_failed"):
                self.skipTest("Position RT unavailable: " + status["error"])
            self.assertEqual(status.get("mode"), "world_position", status)
            target_id = status["target_id"]
            request = mcp_server.call_engine("capture_frame", {
                "target_id": target_id, "attachment": "color_depth",
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
                position_path = result["captures"][0]["metadata_path"]
                depth_path = result["captures"][1]["metadata_path"]
                position_manifest = json.loads(Path(position_path).read_text(encoding="utf-8"))
                depth_manifest = json.loads(Path(depth_path).read_text(encoding="utf-8"))
                self.assertEqual(position_manifest["vk_format"], 109)
                self.assertEqual(position_manifest["encoding"], "rgba32f")
                self.assertEqual(position_manifest["position"]["space"], "world_absolute")
                self.assertEqual(position_manifest["camera"], depth_manifest["camera"])
                self.assertEqual(position_manifest["frame_id"], depth_manifest["frame_id"])
                position = capture_manifest.analyze_capture(position_path, "world_position_rt")
                self.assertTrue(position["valid"], position)
                self.assertGreater(position["point_count"], 0)
                comparison = capture_manifest.analyze_capture(
                    position_path, "position_crosscheck", sample_step=8,
                    depth_metadata_path=depth_path)
                self.assertTrue(comparison["valid"], comparison)
                self.assertGreater(comparison["point_count"], 3)
                self.assertGreaterEqual(len(comparison["distance_bands"]), 2)
                error = comparison["absolute_position_error"]
                self.assertTrue(math.isfinite(error["mean"]), comparison)
                print("position_rt_crosscheck=" + json.dumps({
                    "point_count": comparison["point_count"],
                    "absolute_position_error": error,
                    "distance_bands": comparison["distance_bands"],
                }, allow_nan=False))
            finally:
                self.assertTrue(mcp_server.call_engine("release_capture", {
                    "request_id": request["request_id"],
                }).get("valid"))
        finally:
            mcp_server.call_engine("set_debug_view", {"mode": "off"})

    @unittest.skipUnless(os.environ.get("ULRE_GPU_CULL_TEST") == "1",
                         "requires the static one-board, four-dice ChessboardStress scene")
    def test_production_gpu_cull_draw_count(self):
        for _ in range(300):
            listing = mcp_server.call_engine("list_capture_buffers")
            buffers = [item for item in listing.get("buffers", [])
                       if item["name"] == "GPUCullSystem.DrawCount" and item["available"]]
            if buffers:
                break
            time.sleep(0.1)
        self.assertEqual(len(buffers), 1, listing)
        buffer = buffers[0]
        self.assertEqual(buffer["size"], 4)
        previous_frame = 0
        for _ in range(2):
            request = mcp_server.call_engine("capture_buffer", {
                "resource_id": buffer["resource_id"], "offset": 0, "size": 4,
            })
            self.assertTrue(request["valid"], request)
            try:
                for _ in range(300):
                    result = mcp_server.call_engine("get_capture_status", {
                        "request_id": request["request_id"],
                    })
                    if result.get("state") not in ("queued", "capturing"):
                        break
                    time.sleep(0.1)
                self.assertEqual(result["state"], "succeeded", result)
                self.assertGreater(result["frame_id"], previous_frame)
                previous_frame = result["frame_id"]
                metadata = json.loads(Path(result["metadata_path"]).read_text(encoding="utf-8"))
                self.assertEqual(metadata["resource_id"], buffer["resource_id"])
                self.assertEqual(metadata["frame_id"], result["frame_id"])
                self.assertEqual((metadata["offset"], metadata["byte_count"]), (0, 4))
                data = Path(result["dump_path"]).read_bytes()
                self.assertEqual(len(data), 4)
                self.assertEqual(struct.unpack("<I", data)[0], 5)
            finally:
                mcp_server.call_engine("release_capture", {"request_id": request["request_id"]})

    @unittest.skipUnless(os.environ.get("ULRE_BUFFER_TEST") == "1",
                         "requires a GPU-written SSBO fixture")
    @unittest.skipIf(os.environ.get("ULRE_BUFFER_FAILURE_TEST") == "1",
                     "fixture intentionally fails or destroys the resource")
    def test_registered_storage_buffer(self):
        for _ in range(150):
            listing = mcp_server.call_engine("list_capture_buffers")
            buffers = {item["name"]: item for item in listing.get("buffers", [])}
            if "fixture_ssbo" in buffers and "fixture_no_transfer_src" in buffers:
                break
            time.sleep(0.1)
        self.assertIn("fixture_ssbo", buffers)
        buffer = buffers["fixture_ssbo"]
        unavailable = buffers["fixture_no_transfer_src"]
        self.assertTrue(buffer["available"])
        self.assertFalse(unavailable["available"])
        for resource_id, offset, size, error in (
                (unavailable["resource_id"], 0, 4, "buffer_unavailable"),
                (buffer["resource_id"], 1, 4, "invalid_range"),
                (buffer["resource_id"], 0, 3, "invalid_range"),
                (buffer["resource_id"], 60, 8, "invalid_range"),
                (buffer["resource_id"], 0, 4194308, "invalid_range")):
            response = mcp_server.call_engine("capture_buffer", {
                "resource_id": resource_id, "offset": offset, "size": size,
            })
            self.assertFalse(response["valid"], response)
            self.assertEqual(response["error"], error)
        previous_frame = 0
        for _ in range(2):
            request = mcp_server.call_engine("capture_buffer", {
                "resource_id": buffer["resource_id"], "offset": 4, "size": 12,
            })
            self.assertTrue(request["valid"], request)
            try:
                for _ in range(150):
                    result = mcp_server.call_engine("get_capture_status", {
                        "request_id": request["request_id"],
                    })
                    if result.get("state") not in ("queued", "capturing"):
                        break
                    time.sleep(0.1)
                self.assertEqual(result["state"], "succeeded", result)
                self.assertGreater(result["frame_id"], previous_frame)
                previous_frame = result["frame_id"]
                metadata = json.loads(Path(result["metadata_path"]).read_text(encoding="utf-8"))
                self.assertEqual(metadata["schema"], "ulre.buffer_capture.v1")
                self.assertEqual(metadata["request_id"], request["request_id"])
                self.assertEqual(metadata["resource_id"], buffer["resource_id"])
                self.assertEqual(metadata["frame_id"], result["frame_id"])
                self.assertEqual((metadata["offset"], metadata["byte_count"]), (4, 12))
                self.assertEqual(metadata["buffer_usage"], buffer["usage"])
                self.assertEqual(metadata["byte_order"], "opaque")
                data = Path(result["dump_path"]).read_bytes()
                self.assertEqual(data, struct.pack("<III", *([0x12340000 + result["frame_id"]] * 3)))
                self.assertNotEqual(data, b"\xa5" * 12)
            finally:
                mcp_server.call_engine("release_capture", {"request_id": request["request_id"]})

    @unittest.skipUnless(os.environ.get("ULRE_BUFFER_FAILURE_TEST") == "1",
                         "requires a failing SSBO fixture")
    def test_storage_buffer_failure_or_destruction(self):
        for _ in range(150):
            listing = mcp_server.call_engine("list_capture_buffers")
            buffer = next((item for item in listing.get("buffers", [])
                           if item["name"] == "fixture_ssbo"), None)
            if buffer:
                break
            time.sleep(0.1)
        self.assertIsNotNone(buffer, listing)
        request = mcp_server.call_engine("capture_buffer", {
            "resource_id": buffer["resource_id"], "offset": 0, "size": 4,
        })
        self.assertTrue(request["valid"], request)
        try:
            for _ in range(150):
                result = mcp_server.call_engine("get_capture_status", {
                    "request_id": request["request_id"],
                })
                if result.get("state") not in ("queued", "capturing"):
                    break
                time.sleep(0.1)
            expected = os.environ.get("ULRE_BUFFER_FAILURE_KIND")
            if expected == "destroy":
                self.assertEqual((result["state"], result["error"]), ("cancelled", "target_destroyed"))
                self.assertNotIn(buffer["resource_id"], [item["resource_id"] for item in
                    mcp_server.call_engine("list_capture_buffers")["buffers"]])
            elif expected == "queue":
                self.assertEqual((result["state"], result["error"]), ("failed", "queue_unavailable"))
            else:
                self.assertEqual((result["state"], result["error"]), ("failed", "submission_failed"))
            self.assertFalse(result.get("dump_path"))
        finally:
            mcp_server.call_engine("release_capture", {"request_id": request["request_id"]})

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
