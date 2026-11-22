import json
import os
import tempfile
import unittest
from pathlib import Path

import mcp_server


@unittest.skipUnless(os.environ.get("ULRE_CAPTURE_TEST") == "1",
                     "requires a running engine with DebugBridge enabled")
class CaptureIntegrationTests(unittest.TestCase):
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
