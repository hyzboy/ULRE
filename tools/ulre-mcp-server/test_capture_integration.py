import json
import os
import unittest
from pathlib import Path

import mcp_server


@unittest.skipUnless(os.environ.get("ULRE_CAPTURE_TEST") == "1",
                     "requires a running engine with DebugBridge enabled")
class CaptureIntegrationTests(unittest.TestCase):
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
                finally:
                    released = mcp_server.call_engine("release_capture", {
                        "request_id": request_id,
                    })
                    self.assertTrue(released.get("valid"), released)
