import ctypes
from ctypes import wintypes
import os
import struct
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

import capture_manifest
import mcp_server
import render_regression


@unittest.skipUnless(os.environ.get("ULRE_COVERAGE_FIXTURE_EXE"),
                     "requires the Vulkan diagnostics fixture")
class DebugCoverageIntegrationTests(unittest.TestCase):
    def test_masked_and_dithered_geometry_matches_main_depth(self):
        executable = Path(os.environ["ULRE_COVERAGE_FIXTURE_EXE"]).resolve(strict=True)
        artifact_dir = Path(os.environ["ULRE_COVERAGE_ARTIFACT_DIR"]).resolve(strict=True)
        probe = mcp_server.call_engine("get_debug_view")
        self.assertEqual(probe.get("error"), "ULRE bridge unavailable", probe)
        masks = {}
        for recipe in ("AlphaModes.LitOpaque", "AlphaModes.LitAlphaTest",
                       "AlphaModes.LitAlphaDither"):
            with self.subTest(recipe=recipe):
                masks[recipe] = self._run_scene(executable, artifact_dir, recipe)
        opaque = masks["AlphaModes.LitOpaque"]
        tested = masks["AlphaModes.LitAlphaTest"]
        self.assertGreater(sum(opaque), sum(tested))
        self.assertTrue(all(not masked or full for full, masked in zip(opaque, tested)))
        self._run_scene(executable, artifact_dir, "AlphaModes.LitOpaque", without_normal_map=True)
        self._run_scene(executable, artifact_dir, "AlphaModes.LitOpaque", missing_normal=True)

    def _run_scene(self, executable, artifact_dir, recipe, without_normal_map=False, missing_normal=False):
        with tempfile.TemporaryDirectory(prefix="coverage_", dir=artifact_dir) as temporary:
            directory = Path(temporary)
            render_regression._prepare(directory)
            scene = directory / render_regression.SCENE
            text = scene.read_text(encoding="utf-8")
            self.assertEqual(text.count('recipe = "06b.BasicLit.Lit"'), 3)
            text = text.replace('recipe = "06b.BasicLit.Lit"', f'recipe = "{recipe}"')
            text = text.replace("base_color = [1.0, 1.0, 1.0, 1.0]",
                                "base_color = [1.0, 1.0, 1.0, 0.75]")
            if without_normal_map:
                normal = 'normal = "pack(res):image/Brickwall/Normal.Tex2D"\n'
                self.assertEqual(text.count(normal), 1)
                text = text.replace(normal, "")
            scene.write_text(text, encoding="utf-8")
            env = os.environ.copy()
            for key in list(env):
                if key.upper().startswith("ULRE_"):
                    del env[key]
            env["ULRE_DIAGNOSTIC_PAIRED_CAPTURE"] = "1"
            if missing_normal:
                env["ULRE_DIAGNOSTIC_MISSING_NORMAL"] = "1"
            suffix = "-missing-normal" if missing_normal else "-no-normal-map" if without_normal_map else ""
            log = artifact_dir / f"{recipe}{suffix}.log"
            with log.open("wb") as output:
                process = subprocess.Popen([str(executable)], cwd=directory, env=env,
                                           stdout=output, stderr=subprocess.STDOUT)
                try:
                    render_regression._wait_ready(process, log, "off", 120)
                    targets = render_regression._bridge("list_capture_targets")["targets"]
                    main = next(item["target_id"] for item in targets if item["kind"] == "swapchain")
                    before = self._request(main)
                    before_result = self._wait(before)
                    before_color = self._read(before_result["captures"][0]["metadata_path"])[1]
                    normals = {}
                    masks = {}
                    for mode in ("world_normal", "shading_normal", "world_position"):
                        render_regression._bridge("set_debug_view", {"mode": mode})
                        if recipe == "AlphaModes.LitAlphaDither" or missing_normal:
                            deadline = time.monotonic() + 30
                            while time.monotonic() < deadline:
                                status = render_regression._bridge("get_debug_view")
                                if status.get("error"):
                                    break
                                time.sleep(0.05)
                            self.assertEqual(status.get("error"), "unsupported_scene_batch", status)
                            self.assertEqual(status["mode"], "off", status)
                            self._disable_view()
                            self._close(process)
                            return
                        render_regression._wait_ready(process, log, mode, 120)
                        status = render_regression._bridge("get_debug_view")
                        self.assertEqual(status["mode"], mode, status)
                        self.assertGreaterEqual(status["tracked_entities"], 3)
                        main_request = self._request(main)
                        debug_request = self._request(status["target_id"])
                        main_result = self._wait(main_request)
                        debug_result = self._wait(debug_request)
                        self.assertEqual(main_result["frame_id"], debug_result["frame_id"])
                        main_color_meta, main_color = self._read(main_result["captures"][0]["metadata_path"])
                        main_depth_meta, main_depth = self._read(main_result["captures"][1]["metadata_path"])
                        color_meta, color = self._read(debug_result["captures"][0]["metadata_path"])
                        depth_meta, depth = self._read(debug_result["captures"][1]["metadata_path"])
                        self.assertEqual(main_color, before_color)
                        self.assertEqual(main_depth_meta["encoding"], "r32f")
                        self.assertEqual(depth_meta["encoding"], "r32f")
                        self.assertEqual(main_depth, depth)
                        self.assertEqual(color_meta["frame_id"], depth_meta["frame_id"])
                        self.assertEqual(main_color_meta["frame_id"], color_meta["frame_id"])
                        if mode != "world_position":
                            self.assertEqual(color_meta["normal"]["space"], "world")
                            self.assertEqual(color_meta["normal"]["source"],
                                             "interpolated_vertex" if mode == "world_normal"
                                             else "standard_opaque_shading")
                        stride = 16 if mode == "world_position" else 4
                        clear = depth_meta["clear_depth"]
                        valid = 0
                        coverage = bytearray()
                        for index, (value,) in enumerate(struct.iter_unpack("<f", depth)):
                            covered = value != clear
                            coverage.append(covered)
                            alpha = (struct.unpack_from("<f", color, index * stride + 12)[0]
                                     if stride == 16 else color[index * stride + 3] / 255)
                            self.assertEqual(alpha != 0, covered)
                            valid += covered
                        self.assertGreater(valid, 100)
                        self.assertLess(valid, 20000)
                        masks[mode] = coverage
                        if mode != "world_position":
                            normals[mode] = color
                    if without_normal_map:
                        difference = max(abs(a - b) for a, b in zip(
                            normals["world_normal"], normals["shading_normal"]))
                        self.assertLessEqual(difference, 1)
                    else:
                        self.assertNotEqual(normals["world_normal"], normals["shading_normal"])
                    self.assertEqual(masks["world_normal"], masks["shading_normal"])
                    self.assertEqual(masks["world_normal"], masks["world_position"])
                    self._disable_view()
                    self._close(process)
                    return masks["world_normal"]
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()

    def _disable_view(self):
        render_regression._bridge("set_debug_view", {"mode": "off"})
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            status = render_regression._bridge("get_debug_view")
            if status["mode"] == "off" and not status["target_id"]:
                break
            time.sleep(0.05)
        self.assertEqual(status["mode"], "off", status)
        self.assertFalse(status["target_id"], status)

    def _close(self, process):
        user32 = ctypes.windll.user32
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        user32.EnumWindows.argtypes = (callback_type, wintypes.LPARAM)
        user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
        user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
        windows = []

        @callback_type
        def find_window(window, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(window, ctypes.byref(pid))
            if pid.value == process.pid:
                windows.append(window)
            return True

        self.assertTrue(user32.EnumWindows(find_window, 0))
        self.assertTrue(windows, "fixture window not found")
        for window in windows:
            user32.PostMessageW(window, 0x0010, 0, 0)
        self.assertEqual(process.wait(timeout=30), 0)

    def _request(self, target):
        return render_regression._bridge("capture_frame", {
            "target_id": target, "attachment": "color_depth",
        })["request_id"]

    def _wait(self, request_id):
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                result = render_regression._bridge("get_capture_status", {"request_id": request_id})
                if result["state"] not in ("queued", "capturing"):
                    break
                time.sleep(0.05)
            self.assertEqual(result["state"], "succeeded", result)
            return result
        finally:
            status = mcp_server.call_engine("get_capture_status", {"request_id": request_id})
            if status.get("state") == "queued":
                mcp_server.call_engine("cancel_capture", {"request_id": request_id})
            mcp_server.call_engine("release_capture", {"request_id": request_id})

    @staticmethod
    def _read(path):
        metadata, raw = capture_manifest.load_manifest(path)
        return metadata, raw.read_bytes()
