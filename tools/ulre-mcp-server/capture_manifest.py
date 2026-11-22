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


def analyze_capture(path, operation, color_space_override=None, reversed_z_override=None):
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
    elif operation == "depth":
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
        result = analyzer.analyze_depth(str(data_path), width, height, reversed_z, clear_depth)
    else:
        raise ValueError("Unsupported capture analysis operation")
    result["capture"] = {key: metadata[key] for key in ("request_id", "target_id", "frame_id")}
    result["pixel_scope"] = "full_frame_including_clear"
    return result
