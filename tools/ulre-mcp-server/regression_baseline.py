"""Explicit, device-bound baselines for the render diagnostics fixture."""

import hashlib
import json
import math
import os
import shutil
import tempfile
import uuid
from pathlib import Path

import capture_manifest

SCHEMA = "ulre.render_baseline.v1"
ALGORITHM = "ulre_frame_diff_v2"


class NotCovered(Exception):
    pass


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _profile(metadata):
    device = metadata.get("device")
    if not isinstance(device, dict):
        raise NotCovered("capture_device_identity_unavailable")
    fields = ("vendor_id", "device_id", "device_type", "api_version", "driver_version")
    if any(type(device.get(key)) is not int or device[key] <= 0 for key in fields):
        raise NotCovered("capture_device_identity_incomplete")
    driver = device.get("driver_properties")
    if not isinstance(driver, dict) or type(driver.get("driver_id")) is not int \
            or driver["driver_id"] <= 0 or not driver.get("driver_name"):
        raise NotCovered("capture_driver_identity_unavailable")
    return {key: device[key] for key in fields} | {
        "driver_id": driver["driver_id"],
        "driver_name": driver["driver_name"],
        "driver_info": driver.get("driver_info"),
    }


def _capture(path, scene):
    metadata, data = capture_manifest.load_manifest(path)
    if metadata.get("source") != "debug_view" or metadata.get("attachment") != "color" \
            or metadata.get("debug_view_mode") != scene.get("mode") \
            or metadata.get("mask") != "alpha_nonzero":
        raise NotCovered("capture_does_not_match_fixture_view")
    if (metadata["width"], metadata["height"]) != (scene.get("width"), scene.get("height")):
        raise NotCovered("capture_dimensions_do_not_match_fixture")
    if "capture_frame" in scene and metadata["frame_id"] != scene["capture_frame"]:
        raise NotCovered("capture_frame_does_not_match_fixture")
    if metadata["encoding"] == "rgba32f":
        raise NotCovered("position_rt_requires_numeric_crosscheck")
    if metadata.get("camera_status") != "recorded":
        raise NotCovered("capture_camera_unavailable")
    return metadata, data, _profile(metadata)


def write_baseline(path, capture_path, scene, *, update=False):
    path = Path(path).resolve()
    metadata, data, profile = _capture(capture_path, scene)
    path.parent.mkdir(parents=True, exist_ok=True)
    if update and not path.is_file():
        raise NotCovered("baseline_missing_for_update")
    if not update and path.exists():
        raise FileExistsError("baseline already exists; use update explicitly")
    token = uuid.uuid4().hex
    raw_name = f"baseline_{token}.raw"
    meta_name = f"baseline_{token}.capture.json"
    raw_path = path.parent / raw_name
    meta_path = path.parent / meta_name
    shutil.copyfile(data, raw_path)
    copied = dict(metadata)
    copied["data_file"] = raw_name
    copied["capture_id"] = raw_name
    meta_path.write_text(json.dumps(copied, allow_nan=False), encoding="utf-8")
    descriptor = {
        "schema": SCHEMA, "algorithm": ALGORITHM, "capture_schema": metadata["schema"],
        "scene": scene, "device": profile,
        "format": {key: metadata[key] for key in ("vk_format", "encoding", "color_space")},
        "capture_manifest": meta_name, "capture_manifest_sha256": _sha256(meta_path),
        "data_sha256": _sha256(raw_path),
    }
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                         prefix=".baseline_", delete=False) as output:
            temporary = Path(output.name)
            json.dump(descriptor, output, allow_nan=False, sort_keys=True, indent=2)
            output.flush()
            os.fsync(output.fileno())
        if update:
            os.replace(temporary, path)
        else:
            os.link(temporary, path)
            temporary.unlink()
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return {"status": "PASS", "operation": "update" if update else "create",
            "baseline": str(path), "note": "baseline saved; no comparison performed"}


def compare_baseline(path, capture_path, scene, heatmap_dir, cross_tolerance=None):
    if cross_tolerance is not None and (type(cross_tolerance) not in (int, float)
            or not math.isfinite(cross_tolerance) or not 0 <= cross_tolerance <= 1):
        raise ValueError("cross-device tolerance must be finite and in [0,1]")
    path = Path(path).resolve()
    if not path.is_file():
        raise NotCovered("baseline_missing")
    baseline = json.loads(path.read_text(encoding="utf-8"))
    if baseline.get("schema") != SCHEMA or baseline.get("algorithm") != ALGORITHM \
            or baseline.get("capture_schema") != "ulre.capture.v1":
        raise NotCovered("baseline_schema_or_algorithm_mismatch")
    if baseline.get("scene") != scene:
        raise NotCovered("baseline_scene_or_configuration_mismatch")
    metadata, current_data, device = _capture(capture_path, scene)
    if baseline.get("format") != {key: metadata[key] for key in ("vk_format", "encoding", "color_space")}:
        raise NotCovered("baseline_format_or_color_space_mismatch")
    previous_device = baseline.get("device")
    threshold = 0.0
    if device != previous_device:
        if cross_tolerance is None:
            raise NotCovered("baseline_device_or_driver_mismatch")
        if not isinstance(previous_device, dict) or any(
                previous_device.get(key) != device[key]
                for key in ("vendor_id", "device_type", "driver_id")):
            raise NotCovered("baseline_device_class_mismatch")
        threshold = cross_tolerance
    filename = baseline.get("capture_manifest")
    if not isinstance(filename, str) or Path(filename).name != filename or ":" in filename \
            or "\\" in filename or "/" in filename:
        raise ValueError("invalid baseline capture manifest path")
    saved = path.parent / filename
    if _sha256(saved) != baseline.get("capture_manifest_sha256"):
        raise ValueError("baseline capture manifest digest mismatch")
    _, saved_data = capture_manifest.load_manifest(saved)
    if _sha256(saved_data) != baseline.get("data_sha256"):
        raise ValueError("baseline capture data digest mismatch")
    try:
        result = capture_manifest.compare_captures(
            saved, capture_path, threshold=threshold, heatmap_dir=heatmap_dir,
            allow_unverified_context=True)
    except ValueError as exc:
        return {"status": "FAIL", "reason": str(exc)}
    identical = saved_data.read_bytes() == current_data.read_bytes()
    passed = result["changed_pixels"] == 0 and result["mask"]["coverage_changed_pixels"] == 0
    if device == previous_device:
        passed = passed and identical
    return {"status": "PASS" if passed else "FAIL", "comparison": result,
            "context_verification": "scene, format and device verified by baseline tool",
            "comparison_policy": "strict" if device == previous_device else "declared_cross_device_tolerance",
            "tolerance": threshold, "byte_identical": identical}
