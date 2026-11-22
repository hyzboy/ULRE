"""Run the fixed diagnostic fixture and explicitly create, update or compare a baseline."""

import argparse
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import capture_manifest
import mcp_server
import regression_baseline

ROOT = Path(__file__).resolve().parents[2]
SCENE = Path("example") / "Debug" / "RenderDiagnosticsFixture.level.toml"
INPUTS = [
    SCENE,
    Path("res") / "geo" / "presets" / "plane_square.geo.toml",
    Path("res") / "geo" / "presets" / "cube.geo.toml",
    Path("res") / "geo" / "presets" / "sphere_64.geo.toml",
    Path("res") / "image" / "Brickwall" / "Albedo.Tex2D",
    Path("res") / "image" / "Brickwall" / "Normal.Tex2D",
    Path("res") / "image" / "Brickwall" / "Roughness.Tex2D",
    Path("ShaderLibrary") / "asset" / "recipe" / "Examples" / "Material" / "06b.BasicLit.Lit.recipe.toml",
    Path("example") / "Debug" / "RenderDiagnosticsFixture.cpp",
    Path("GLSLCompiler.dll"),
]
MODES = ("world_normal", "base_color", "surface", "emissive", "uv0", "world_position")
FAULTS = ("camera", "normal", "material", "coverage")


def _digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for data in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def scene_contract(mode):
    return {
        "name": "RenderDiagnosticsFixture", "version": 3, "mode": mode,
        "width": 640, "height": 360, "fixed_delta_seconds": 1.0 / 60.0,
        "initial_warmup_frames": 120, "view_warmup_frames": 120, "capture_frame": 241,
        "light_direction": [0.55, 0.35, 0.75], "light_intensity": 1.5,
        "shadow_cascades": 1, "shadow_map_size": 1024,
        "validation_layers": os.environ.get("VK_INSTANCE_LAYERS", ""),
        "inputs_sha256": {str(path).replace("\\", "/"): _digest(ROOT / path) for path in INPUTS},
    }


def _bridge(method, params=None):
    value = mcp_server.call_engine(method, params)
    if not value.get("valid"):
        raise RuntimeError(f"{method}: {value}")
    return value


def _wait_ready(process, log, mode, timeout):
    deadline = time.monotonic() + timeout
    marker = f"DIAGNOSTIC_FIXTURE_READY mode={mode} ".encode("ascii")
    offset = 0
    tail = b""
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise regression_baseline.NotCovered(f"fixture_exited_{process.returncode}")
        with log.open("rb") as source:
            source.seek(offset)
            chunk = source.read()
            offset = source.tell()
        tail += chunk
        if marker in tail:
            return
        tail = tail[-256:]
        time.sleep(0.05)
    raise regression_baseline.NotCovered(f"fixture_{mode}_warmup_timeout")


def _prepare(directory, fault=None):
    (directory / "ULRE_DEBUG.toml").write_text("[channels]\ndebugbridge = true\n", encoding="utf-8")
    (directory / "asset_mount.toml").write_text(
        '[[mount]]\nname = "res"\nroot = ' + json.dumps(str(ROOT / "res")) +
        '\nsave_manifest = false\n\n[[mount]]\nname = "ShaderLibrary"\nroot = ' +
        json.dumps(str(ROOT / "ShaderLibrary")) + '\nload_manifest = false\nsave_manifest = false\n',
        encoding="utf-8")
    shutil.copyfile(ROOT / "GLSLCompiler.dll", directory / "GLSLCompiler.dll")
    for relative in INPUTS[:4]:
        target = directory / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, target)
    if fault:
        path = directory / SCENE
        text = path.read_text(encoding="utf-8")
        replacements = {
            "camera": ("yaw = 45.0", "yaw = 60.0"),
            "material": ("roughness = 0.92", "roughness = 0.10"),
            "coverage": ("transform.position = [3.0, 0.0, 1.0]",
                         "transform.position = [7.0, 0.0, 1.0]"),
        }
        if fault == "normal":
            before = ('transform.position = [0.0, 0.0, 1.0]\n'
                      'transform.rotation_axis = [0.0, 0.0, 1.0]\n'
                      'transform.rotation_angle_deg = 0.0')
            after = ('transform.position = [0.0, 0.0, 1.0]\n'
                     'transform.rotation_axis = [0.0, 1.0, 0.0]\n'
                     'transform.rotation_angle_deg = 45.0')
        else:
            before, after = replacements[fault]
        if text.count(before) != 1:
            raise RuntimeError("fault_injection_fixture_mismatch")
        path.write_text(text.replace(before, after), encoding="utf-8")


def _copy_capture(path, output_dir):
    metadata, data = capture_manifest.load_manifest(path)
    destination = output_dir / data.name
    with destination.open("xb") as target, data.open("rb") as source:
        shutil.copyfileobj(source, target)
    manifest_path = output_dir / Path(path).name
    with manifest_path.open("xb") as target, Path(path).open("rb") as source:
        shutil.copyfileobj(source, target)
    return manifest_path, metadata


def _analyze_fixture(color, depth, mode):
    if mode == "world_position":
        return {
            "position": capture_manifest.analyze_capture(color, "world_position_rt"),
            "crosscheck": capture_manifest.analyze_capture(
                color, "position_crosscheck", depth_metadata_path=str(depth), sample_step=8),
        }
    operation = {"world_normal": "normals", "surface": "surface"}.get(mode, "luminance")
    return capture_manifest.analyze_capture(color, operation)


def _check_fixture_analysis(analysis, mode):
    if mode == "world_position":
        if analysis["crosscheck"]["point_count"] <= 0:
            raise RuntimeError("position_crosscheck_has_no_samples")
        return
    if analysis["sample_count"] <= 0 or analysis.get("nan_count", 0) or analysis.get("inf_count", 0):
        raise RuntimeError("fixture_has_no_finite_samples")
    if mode == "world_normal" and (analysis["zero_normal_ratio"] != 0 or
                                   analysis["length_error_ratio"] > 0.01):
        raise RuntimeError("fixture_normal_length_out_of_range")
    if mode == "surface" and abs(analysis["metallic"]["mean"] - 0.08) > 1.0 / 255.0:
        raise RuntimeError("fixture_metallic_out_of_range")


def _check_known_normal_pixels(path):
    metadata, data_path = capture_manifest.load_manifest(path)
    if metadata.get("debug_view_mode") != "world_normal" or metadata["encoding"] != "rgba8":
        raise RuntimeError("known_normal_requires_rgba8_world_normal")
    camera = metadata.get("camera")
    vp = camera.get("vp") if isinstance(camera, dict) else None
    if not isinstance(vp, list) or len(vp) != 16 or not all(
            isinstance(value, (int, float)) and math.isfinite(value) for value in vp):
        raise RuntimeError("known_normal_camera_unavailable")
    viewport = metadata["viewport"]
    pixels = data_path.read_bytes()
    checked = []
    for label, z in (("floor_center", 0.0), ("cube_top_center", 1.5)):
        point = (0.0, 0.0, z, 1.0)
        clip = [sum(vp[column * 4 + row] * point[column] for column in range(4))
                for row in range(4)]
        if clip[3] <= 0:
            raise RuntimeError("known_normal_anchor_behind_camera")
        x = math.floor(viewport["x"] + (clip[0] / clip[3] + 1) * viewport["width"] / 2)
        y = math.floor(viewport["y"] + (clip[1] / clip[3] + 1) * viewport["height"] / 2)
        if not (0 <= x < metadata["width"] and 0 <= y < metadata["height"]):
            raise RuntimeError("known_normal_anchor_outside_capture")
        offset = y * metadata["row_pitch"] + x * 4
        rgba = pixels[offset:offset + 4]
        if len(rgba) != 4 or rgba[3] != 255 or any(
                abs(rgba[i] - expected) > 2 for i, expected in enumerate((128, 128, 255))):
            raise RuntimeError(f"known_normal_direction_mismatch:{label}:{x},{y}")
        checked.append({"anchor": label, "pixel": [x, y], "expected_world_normal": [0, 0, 1]})
    return checked


def _capture_fixture(exe, mode, output_dir, timeout, fault=None):
    if os.name != "nt":
        raise regression_baseline.NotCovered("windows_named_pipe_required")
    probe = mcp_server.call_engine("get_debug_view")
    if probe.get("valid") or probe.get("error") != "ULRE bridge unavailable":
        raise regression_baseline.NotCovered("diagnostic_pipe_already_in_use_or_unverifiable")
    scene = scene_contract(mode)
    output_dir.mkdir(parents=True, exist_ok=True)
    log = output_dir / f"fixture_{time.time_ns()}.log"
    error_log = log.with_suffix(".err")
    with tempfile.TemporaryDirectory(prefix="ulre_regression_") as temporary:
        directory = Path(temporary)
        _prepare(directory, fault)
        for relative in INPUTS[:4]:
            if fault and relative == SCENE:
                continue
            if _digest(directory / relative) != scene["inputs_sha256"][str(relative).replace("\\", "/")]:
                raise RuntimeError("copied_fixture_input_mismatch")
        env = os.environ.copy()
        for key in list(env):
            if key.upper().startswith("ULRE_"):
                del env[key]
        with log.open("wb") as stdout, error_log.open("wb") as stderr:
            process = subprocess.Popen([str(exe)], cwd=directory, env=env,
                                       stdout=stdout, stderr=stderr)
            request_id = None
            try:
                _wait_ready(process, log, "off", timeout)
                _bridge("set_debug_view", {"mode": mode})
                _wait_ready(process, log, mode, timeout)
                status = _bridge("get_debug_view")
                if status.get("error"):
                    raise regression_baseline.NotCovered("debug_view_" + status["error"])
                if status.get("mode") != mode or not status.get("target_id"):
                    raise regression_baseline.NotCovered("debug_view_not_ready")
                request = _bridge("capture_frame", {
                    "target_id": status["target_id"], "attachment": "color_depth",
                })
                request_id = request["request_id"]
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    result = _bridge("get_capture_status", {"request_id": request_id})
                    if result.get("state") not in ("queued", "capturing"):
                        break
                    if process.poll() is not None:
                        raise RuntimeError(f"fixture exited during capture: {process.returncode}")
                    time.sleep(0.05)
                else:
                    raise RuntimeError("capture_timeout")
                if result.get("state") != "succeeded":
                    raise RuntimeError(f"capture_failed: {result}")
                if result.get("request_id") != request_id or result.get("frame_id") != 241:
                    raise RuntimeError(f"unexpected_capture_frame_or_request: {result}")
                entries = result.get("captures", [])
                if len(entries) != 2 or [entry.get("attachment") for entry in entries] != ["color", "depth"]:
                    raise RuntimeError("capture_missing_color_depth_pair")
                color, color_metadata = _copy_capture(entries[0]["metadata_path"], output_dir)
                depth, depth_metadata = _copy_capture(entries[1]["metadata_path"], output_dir)
                if color_metadata.get("device") != depth_metadata.get("device") or \
                        color_metadata.get("frame_id") != depth_metadata.get("frame_id") or \
                        color_metadata.get("capture_group_id") != depth_metadata.get("capture_group_id"):
                    raise RuntimeError("capture_pair_context_mismatch")
                if scene_contract(mode) != scene:
                    raise RuntimeError("fixture_inputs_changed_during_run")
                return color, depth, scene, log
            finally:
                if request_id is not None:
                    status = mcp_server.call_engine("get_capture_status", {"request_id": request_id})
                    if status.get("state") == "queued":
                        mcp_server.call_engine("cancel_capture", {"request_id": request_id})
                    if status.get("state") in ("succeeded", "failed", "cancelled"):
                        mcp_server.call_engine("release_capture", {"request_id": request_id})
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("create", "update", "compare"))
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--mode", choices=MODES, default="world_normal")
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--cross-device-tolerance", type=float)
    parser.add_argument("--inject-fault", choices=FAULTS,
                        help="modify the staged scene for a negative comparison only")
    args = parser.parse_args(argv)
    evidence = {}
    try:
        if not math.isfinite(args.timeout) or args.timeout <= 0:
            raise ValueError("timeout must be positive")
        if args.operation == "compare" and not args.baseline.is_file():
            raise regression_baseline.NotCovered("baseline_missing")
        if args.operation != "compare" and args.cross_device_tolerance is not None:
            raise ValueError("cross-device tolerance applies only to compare")
        if args.inject_fault and args.operation != "compare":
            raise ValueError("fault injection applies only to compare")
        if args.inject_fault == "material" and args.mode != "surface":
            raise ValueError("material fault requires surface mode")
        if args.inject_fault == "normal" and args.mode != "world_normal":
            raise ValueError("normal fault requires world_normal mode")
        exe = args.exe.resolve(strict=True)
        if exe.name.lower() != "renderdiagnosticsfixture.exe":
            raise ValueError("exe must be RenderDiagnosticsFixture.exe")
        color, depth, scene, log = _capture_fixture(
            exe, args.mode, args.output_dir.resolve(), args.timeout, args.inject_fault)
        evidence = {"color_manifest": str(color), "depth_manifest": str(depth),
                    "fixture_log": str(log), "fixture_exe_sha256": _digest(exe),
                    "fault_injection": args.inject_fault,
                    "limitations": ["alpha_test_not_supported", "receiver_shadow_factor_unavailable",
                                    "shadow_map_capture_not_covered"]}
        analysis = _analyze_fixture(color, depth, args.mode)
        evidence["fixture_analysis"] = analysis
        analysis_error = None
        try:
            _check_fixture_analysis(analysis, args.mode)
            if args.mode == "world_normal":
                evidence["known_normal_pixels"] = _check_known_normal_pixels(color)
        except RuntimeError as exc:
            analysis_error = str(exc)
        if analysis_error and args.operation != "compare":
            raise RuntimeError(analysis_error)
        if args.operation == "compare":
            result = regression_baseline.compare_baseline(
                args.baseline, color, scene, args.output_dir, args.cross_device_tolerance)
            if analysis_error:
                result["status"] = "FAIL"
                result["fixture_assertion_error"] = analysis_error
        else:
            result = regression_baseline.write_baseline(
                args.baseline, color, scene, update=args.operation == "update")
    except regression_baseline.NotCovered as exc:
        result = {"status": "NOT_COVERED", "reason": str(exc)}
    except (OSError, ValueError, RuntimeError, KeyError, subprocess.SubprocessError) as exc:
        result = {"status": "FAIL", "reason": str(exc)}
    result.update(evidence)
    print(json.dumps(result, allow_nan=False, sort_keys=True))
    return {"PASS": 0, "FAIL": 1, "NOT_COVERED": 2}[result["status"]]


if __name__ == "__main__":
    sys.exit(main())
