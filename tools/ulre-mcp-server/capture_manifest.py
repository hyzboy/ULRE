import json
import math
from pathlib import Path

import analyzer

MAX_MANIFEST_BYTES = 1024 * 1024
MAX_CAPTURE_BYTES = 256 * 1024 * 1024
PIXEL_SIZES = {"rgba8": 4, "bgra8": 4, "a2bgr10": 4,
               "a2r10g10b10": 4, "r32f": 4}
VK_FORMAT_LAYOUTS = {
    37: ("rgba8", "color", "unknown"),
    43: ("rgba8", "color", "srgb"),
    44: ("bgra8", "color", "unknown"),
    50: ("bgra8", "color", "srgb"),
    58: ("a2r10g10b10", "color", "unknown"),
    64: ("a2bgr10", "color", "unknown"),
    126: ("r32f", "depth", "unknown"),
    130: ("r32f", "depth", "unknown"),
}


def _positive_int(value, name):
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


def _finite_number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def load_manifest(path):
    metadata_path = Path(path).resolve(strict=True)
    if metadata_path.stat().st_size > MAX_MANIFEST_BYTES:
        raise ValueError("Capture manifest exceeds the size limit")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if not isinstance(metadata, dict) or metadata.get("schema") != "ulre.capture.v1":
        raise ValueError("Unsupported capture manifest schema")
    if metadata.get("source") != "swapchain":
        raise ValueError("Unsupported capture source")
    for field in ("request_id", "target_id", "frame_id", "width", "height",
                  "pixel_size", "row_pitch", "byte_count"):
        _positive_int(metadata.get(field), field)
    if type(metadata.get("samples")) is not int or metadata["samples"] != 1 or \
            type(metadata.get("mip")) is not int or metadata["mip"] != 0 or \
            type(metadata.get("layer")) is not int or metadata["layer"] != 0 or \
            metadata.get("byte_order") != "little":
        raise ValueError("Unsupported capture layout")
    encoding = metadata.get("encoding")
    if encoding not in PIXEL_SIZES or metadata["pixel_size"] != PIXEL_SIZES[encoding]:
        raise ValueError("Unsupported or inconsistent pixel encoding")
    vk_format = metadata.get("vk_format")
    layout = VK_FORMAT_LAYOUTS.get(vk_format) if type(vk_format) is int else None
    if layout != (encoding, metadata.get("attachment"), metadata.get("color_space")):
        raise ValueError("VkFormat, encoding, attachment and color space disagree")
    if metadata["row_pitch"] != metadata["width"] * metadata["pixel_size"]:
        raise ValueError("Padded rows are not supported")
    if metadata["byte_count"] != metadata["row_pitch"] * metadata["height"] or \
            metadata["byte_count"] > MAX_CAPTURE_BYTES:
        raise ValueError("Invalid capture byte count")
    filename = metadata.get("data_file")
    if not isinstance(filename, str) or not filename or filename in (".", "..") or \
            "/" in filename or "\\" in filename or ":" in filename:
        raise ValueError("data_file must be a file name in the manifest directory")
    data_path = (metadata_path.parent / filename).resolve(strict=True)
    if data_path.parent != metadata_path.parent or not data_path.is_file():
        raise ValueError("Capture data must be in the manifest directory")
    if data_path.stat().st_size != metadata["byte_count"]:
        raise ValueError("Capture data size does not match the manifest")
    return metadata, data_path


def analyze_capture(path, operation, color_space_override=None, reversed_z_override=None,
                    sample_step=16, allow_unverified_depth_producers=False):
    metadata, data_path = load_manifest(path)
    width, height = metadata["width"], metadata["height"]
    encoding = metadata["encoding"]
    if operation == "luminance":
        if metadata.get("attachment") != "color":
            raise ValueError("Luminance requires a color capture")
        color_space = metadata.get("color_space")
        if color_space_override is not None:
            if color_space_override not in ("linear", "srgb"):
                raise ValueError("Invalid color space override")
            if color_space != "unknown" and color_space != color_space_override:
                raise ValueError("Color space override conflicts with capture metadata")
            color_space = color_space_override
        if color_space not in ("linear", "srgb"):
            raise ValueError("Capture color space is unknown; supply color_space_override")
        result = analyzer.analyze_luminance(str(data_path), width, height, encoding, color_space)
    elif operation in ("depth", "world_position"):
        if metadata.get("attachment") != "depth" or encoding != "r32f" or \
                metadata.get("copy_aspect") != "depth":
            raise ValueError("Depth analysis requires an R32F depth-aspect capture")
        clear_depth = metadata.get("clear_depth")
        if isinstance(clear_depth, bool) or not isinstance(clear_depth, (int, float)) or \
                not math.isfinite(clear_depth) or not 0 <= clear_depth <= 1:
            raise ValueError("Capture clear_depth is missing or invalid")
        camera = metadata.get("camera") if metadata.get("camera_status") == "recorded" else None
        camera_reversed = camera.get("reversed_z") if isinstance(camera, dict) else None
        if reversed_z_override is not None and not isinstance(reversed_z_override, bool):
            raise ValueError("reversed_z_override must be boolean")
        if isinstance(camera_reversed, bool):
            if reversed_z_override is not None and reversed_z_override != camera_reversed:
                raise ValueError("Reversed-Z override conflicts with capture metadata")
            reversed_z = camera_reversed
        elif reversed_z_override is not None:
            reversed_z = reversed_z_override
        else:
            raise ValueError("Capture has no camera depth convention; supply reversed_z_override")
        if operation == "depth":
            result = analyzer.analyze_depth(str(data_path), width, height, reversed_z, clear_depth)
        else:
            if not isinstance(camera, dict) or camera.get("matrix_layout") != "column_major" or \
                    camera.get("position_space") != "world_absolute":
                raise ValueError("A same-frame absolute-world camera matrix is required")
            if allow_unverified_depth_producers is not True:
                raise ValueError("Depth producers are not fully verified; explicit opt-in is required")
            inverse_vp = camera.get("inverse_vp")
            if not isinstance(inverse_vp, list):
                raise ValueError("Capture inverse VP is missing")
            viewport = metadata.get("viewport")
            if not isinstance(viewport, dict):
                raise ValueError("Capture viewport is missing")
            result = analyzer.reconstruct_world_pos(
                str(data_path), width, height, inverse_vp, reversed_z, sample_step,
                clear_depth, viewport)
            result["position_space"] = "world_absolute"
            result["sampling_step"] = sample_step
            result["limitations"] = ["camera snapshot covers primitive collection only",
                                     "clear-depth filtering cannot distinguish geometry at clear depth"]
    else:
        raise ValueError("Unsupported capture analysis operation")
    result["capture"] = {key: metadata[key] for key in ("request_id", "target_id", "frame_id")}
    result["pixel_scope"] = ("sampled_non_clear_depth" if operation == "world_position"
                             else "full_frame_including_clear")
    return result


def compare_captures(path_a, path_b, threshold=0.01, heatmap_dir=".ulre-analysis",
                     color_space_override=None, allow_unverified_context=False):
    a, data_a = load_manifest(path_a)
    b, data_b = load_manifest(path_b)
    if a["attachment"] != "color" or b["attachment"] != "color":
        raise ValueError("Capture comparison currently supports color attachments only")
    for field in ("width", "height", "encoding", "vk_format", "color_space", "viewport"):
        if a.get(field) != b.get(field):
            raise ValueError(f"Capture {field} differs")
    viewport = a.get("viewport")
    if not isinstance(viewport, dict) or any(
            not _finite_number(viewport.get(key))
            for key in ("x", "y", "width", "height", "min_depth", "max_depth")):
        raise ValueError("Capture viewport is missing or invalid")
    if viewport["x"] < 0 or viewport["y"] < 0 or viewport["width"] <= 0 or \
            viewport["height"] <= 0 or viewport["x"] + viewport["width"] > a["width"] or \
            viewport["y"] + viewport["height"] > a["height"] or \
            not 0 <= viewport["min_depth"] < viewport["max_depth"] <= 1:
        raise ValueError("Capture viewport is outside the image")
    color_space = a["color_space"]
    if color_space_override is not None:
        if color_space_override not in ("linear", "srgb"):
            raise ValueError("Invalid color space override")
        if color_space != "unknown" and color_space != color_space_override:
            raise ValueError("Color space override conflicts with capture metadata")
        color_space = color_space_override
    if color_space not in ("linear", "srgb"):
        raise ValueError("Capture color space is unknown; supply color_space_override")
    if a.get("camera_status") != "recorded" or b.get("camera_status") != "recorded":
        raise ValueError("Comparison requires recorded cameras")
    camera_a, camera_b = a.get("camera"), b.get("camera")
    if not isinstance(camera_a, dict) or not isinstance(camera_b, dict) or \
            camera_a.get("matrix_layout") != "column_major" or \
            camera_b.get("matrix_layout") != "column_major" or \
            not isinstance(camera_a.get("vp"), list) or len(camera_a["vp"]) != 16 or \
            not all(_finite_number(value) for value in camera_a["vp"]) or \
            camera_a["vp"] != camera_b.get("vp"):
        raise ValueError("Capture cameras differ or are invalid")
    if allow_unverified_context is not True:
        raise ValueError("Scene and device context are not recorded; explicit opt-in is required")
    result = analyzer.compare_frames(str(data_a), str(data_b), a["width"], a["height"],
                                     a["encoding"], threshold, heatmap_dir)
    result["captures"] = [{key: entry[key] for key in ("request_id", "target_id", "frame_id")}
                          for entry in (a, b)]
    result["color_space"] = color_space
    result["comparison_space"] = "encoded_channels"
    result["pixel_scope"] = "full_frame_including_clear"
    result["limitations"] = ["scene and device context are unverified",
                             "valid geometry mask is unavailable"]
    return result
