import json
import os
import sys
import time

import analyzer
import capture_manifest


PIPE_NAME = r"\\.\pipe\ulre_render_debug"
MAX_PIPE_REQUEST_BYTES = 4095
MAX_PIPE_RESPONSE_BYTES = 1024 * 1024


def open_engine_pipe():
    deadline = time.monotonic() + 2.0
    while True:
        try:
            return open(PIPE_NAME, "r+b", buffering=0)
        except OSError as exc:
            code = getattr(exc, "winerror", None)
            if code not in (2, 231) and not (code is None and exc.errno in (2, 22)):
                raise
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


TOOLS = [
    {
        "name": "compare_captures",
        "description": "Compare compatible capture manifests using RGB MAE/RMSE/PSNR, fixed-window SSIM and 4-connected changed regions. Image metrics use the valid-mask intersection; coverage changes are reported separately. Requires explicit opt-in for unverified scene/device context.",
        "inputSchema": {"type": "object", "properties": {
            "metadata_path_a": {"type": "string"},
            "metadata_path_b": {"type": "string"},
            "threshold": {"type": "number", "minimum": 0, "default": 0.01},
            "heatmap_dir": {"type": "string"},
            "color_space_override": {"type": "string", "enum": ["linear", "srgb"]},
            "allow_unverified_context": {"type": "boolean", "default": False},
        }, "required": ["metadata_path_a", "metadata_path_b"]},
    },
    {
        "name": "analyze_capture",
        "description": "Analyze a versioned capture manifest, including depth-based world-position bounds.",
        "inputSchema": {"type": "object", "properties": {
            "metadata_path": {"type": "string"},
            "operation": {"type": "string", "enum": ["luminance", "depth", "world_position",
                                                      "world_position_rt", "position_crosscheck",
                                                      "normals", "surface"]},
            "color_space_override": {"type": "string", "enum": ["linear", "srgb"]},
            "reversed_z_override": {"type": "boolean"},
            "sample_step": {"type": "integer", "minimum": 1, "default": 16},
            "allow_unverified_depth_producers": {"type": "boolean", "default": False},
            "mask_metadata_path": {"type": "string"},
            "depth_metadata_path": {"type": "string"},
        }, "required": ["metadata_path", "operation"]},
    },
    {
        "name": "analyze_shadow_semantics",
        "description": "Distinguish shadow-map caster coverage from screen-space receiver shadowing; the latter needs a same-frame receiver shadow factor or mask.",
        "inputSchema": {"type": "object", "properties": {
            "metadata_path": {"type": "string"},
            "is_shadow_map_depth": {"type": "boolean", "default": False},
        }, "required": ["metadata_path"]},
    },
    {
        "name": "get_render_health",
        "description": "Read the ULRE render health ledger.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "get_pass_metrics",
        "description": "Read the latest GPU pass timestamp and pipeline statistics.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "dump_render_target",
        "description": "Read the latest requested render-target capture summary.",
        "inputSchema": {
            "type": "object",
            "properties": {"name": {"type": "string"}},
        },
    },
    {
        "name": "dump_buffer",
        "description": "Read the latest requested SSBO or UBO capture summary.",
        "inputSchema": {
            "type": "object",
            "properties": {"name": {"type": "string"}},
        },
    },
    {
        "name": "inspect_camera_viewport",
        "description": "Read camera and viewport contract violations.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "get_camera_matrices",
        "description": "Read the latest camera VP matrices and depth contract.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "set_debug_view",
        "description": "Request an isolated offscreen attribute view. Alpha test supports PBRSurface and bound VF_V4F vertex color streams; dither and blending are unsupported. world_normal is the interpolated vertex normal; shading_normal includes StandardOpaque normal mapping. Read target_id and errors from get_debug_view.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "mode": {
                    "type": "string",
                    "enum": ["off", "world_normal", "shading_normal", "base_color", "surface", "emissive", "uv0",
                             "world_position"],
                },
            },
            "required": ["mode"],
        },
    },
    {
        "name": "get_debug_view",
        "description": "Read the submitted offscreen debug view, target_id and any unsupported-scene error.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "list_capture_targets",
        "description": "List registered swapchain, ordinary offscreen and debug-view targets.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "list_capture_buffers",
        "description": "List explicitly registered storage buffers and their readback capabilities.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "capture_buffer",
        "description": "Queue a bounded, aligned range from a registered GPU storage buffer.",
        "inputSchema": {"type": "object", "properties": {
            "resource_id": {"type": "integer", "minimum": 1},
            "offset": {"type": "integer", "minimum": 0},
            "size": {"type": "integer", "minimum": 4, "maximum": 4194304},
        }, "required": ["resource_id", "offset", "size"]},
    },
    {
        "name": "capture_frame",
        "description": "Queue target color, depth, or same-frame color and depth capture.",
        "inputSchema": {"type": "object", "properties": {
            "target_id": {"type": "integer", "minimum": 1},
            "attachment": {"type": "string", "enum": ["color", "depth", "color_depth"]},
        }, "required": ["target_id", "attachment"]},
    },
    {
        "name": "get_capture_status",
        "description": "Get the state and result of a queued capture request.",
        "inputSchema": {"type": "object", "properties": {
            "request_id": {"type": "integer", "minimum": 1},
        }, "required": ["request_id"]},
    },
    {
        "name": "cancel_capture",
        "description": "Cancel a queued capture request; in-progress readbacks cannot be cancelled.",
        "inputSchema": {"type": "object", "properties": {
            "request_id": {"type": "integer", "minimum": 1},
        }, "required": ["request_id"]},
    },
    {
        "name": "release_capture",
        "description": "Release a completed capture's in-memory status slot; does not delete its file.",
        "inputSchema": {"type": "object", "properties": {
            "request_id": {"type": "integer", "minimum": 1},
        }, "required": ["request_id"]},
    },
    {
        "name": "analyze_luminance",
        "description": "Legacy raw-capture interface. Prefer analyze_capture with a capture manifest; raw callers must provide dimensions, pixel format and actual color space.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "enum": ["rgba8", "bgra8", "rgb8", "a2bgr10",
                                                          "rgb10a2", "a2r10g10b10",
                                                          "r32f", "rgba32f"]},
            "color_space": {"type": "string", "enum": ["linear", "srgb"]},
            "row_pitch": {"type": "integer", "minimum": 1},
            "mask_alpha": {"type": "boolean", "default": False},
        }, "required": ["path", "width", "height", "color_space"]},
    },
    {
        "name": "analyze_normals",
        "description": "Legacy raw-capture interface. Prefer analyze_capture with a world_normal manifest and its valid-pixel mask.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "enum": ["rgba8", "bgra8", "rgb8", "a2bgr10",
                                                          "rgb10a2", "a2r10g10b10", "rgba32f"]},
            "row_pitch": {"type": "integer", "minimum": 1},
            "mask_alpha": {"type": "boolean", "default": False},
        }, "required": ["path", "width", "height"]},
    },
    {
        "name": "analyze_surface",
        "description": "Legacy raw-capture interface. Prefer analyze_capture with a surface manifest and its valid-pixel mask; endpoint ratios are descriptive, not failures.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "enum": ["rgba8", "bgra8", "rgb8", "a2bgr10",
                                                          "rgb10a2", "a2r10g10b10", "rgba32f"]},
            "row_pitch": {"type": "integer", "minimum": 1},
            "mask_alpha": {"type": "boolean", "default": False},
        }, "required": ["path", "width", "height"]},
    },
    {
        "name": "analyze_depth",
        "description": "Quantify depth distribution and invalid depth pixels.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "reversed_z": {"type": "boolean", "default": False},
            "clear_depth": {"type": "number", "minimum": 0, "maximum": 1},
            "row_pitch": {"type": "integer", "minimum": 1},
        }, "required": ["path", "width", "height"]},
    },
    {
        "name": "reconstruct_world_pos",
        "description": "Legacy raw-depth interface; prefer analyze_capture with depth metadata and a same-frame mask. Manual callers must provide the matching inverse VP and row pitch when padded; never uses the latest camera implicitly.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "sample_step": {"type": "integer", "minimum": 1, "default": 16},
            "row_pitch": {"type": "integer", "minimum": 1},
            "clear_depth": {"type": "number", "minimum": 0, "maximum": 1},
            "inverse_vp": {"type": "array", "items": {"type": "number"},
                           "minItems": 16, "maxItems": 16},
            "reversed_z": {"type": "boolean", "default": False},
            "viewport": {"type": "object", "properties": {
                "x": {"type": "number"}, "y": {"type": "number"},
                "width": {"type": "number", "exclusiveMinimum": 0},
                "height": {"type": "number", "exclusiveMinimum": 0},
                "min_depth": {"type": "number"}, "max_depth": {"type": "number"},
            }, "required": ["x", "y", "width", "height", "min_depth", "max_depth"]},
        }, "required": ["path", "width", "height", "inverse_vp"]},
    },
    {
        "name": "compare_frames",
        "description": "Legacy raw-frame interface; prefer compare_captures with manifests. Computes per-channel and aggregate RGB MAE/RMSE/PSNR, 8x8 non-overlapping SSIM, 4-connected threshold regions and a separate valid-mask coverage delta. Values use encoded channel units, or float channel units for RGBA32F.",
        "inputSchema": {"type": "object", "properties": {
            "path_a": {"type": "string"},
            "path_b": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "enum": ["rgba8", "bgra8", "rgb8", "a2bgr10",
                                                          "rgb10a2", "a2r10g10b10", "rgba32f"]},
            "threshold": {"type": "number", "minimum": 0.0, "default": 0.01},
            "row_pitch_a": {"type": "integer", "minimum": 1},
            "row_pitch_b": {"type": "integer", "minimum": 1},
            "mask_alpha": {"type": "boolean", "default": False},
            "peak_value": {"type": "number", "exclusiveMinimum": 0, "default": 1.0},
        }, "required": ["path_a", "path_b", "width", "height"]},
    },
]


def call_engine(method, params=None):
    if os.name != "nt":
        return {"valid": False, "error": "ULRE named pipe is available on Windows only"}

    payload = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        payload["params"] = params
    request = json.dumps(payload).encode("utf-8")
    if len(request) > MAX_PIPE_REQUEST_BYTES:
        return {"valid": False, "error": "ULRE bridge request exceeds the size limit"}
    try:
        with open_engine_pipe() as pipe:
            if pipe.write(request) != len(request):
                return {"valid": False, "error": "Incomplete ULRE bridge request"}
            chunks = bytearray()
            while len(chunks) < MAX_PIPE_RESPONSE_BYTES:
                data = pipe.read(min(4096, MAX_PIPE_RESPONSE_BYTES - len(chunks)))
                if not data:
                    break
                chunks.extend(data)
                try:
                    envelope = json.loads(chunks)
                except (json.JSONDecodeError, UnicodeDecodeError):
                    continue
                if not isinstance(envelope, dict) or envelope.get("id") != 1:
                    return {"valid": False, "error": "Invalid ULRE bridge response ID"}
                if "error" in envelope:
                    return {"valid": False, "error": "ULRE bridge RPC error",
                            "detail": envelope["error"]}
                result = envelope.get("result")
                if not isinstance(result, dict):
                    return {"valid": False, "error": "Invalid ULRE bridge result"}
                return result
    except OSError as exc:
        return {"valid": False, "error": "ULRE bridge unavailable", "detail": str(exc)}
    return {"valid": False, "error": "Incomplete or oversized ULRE bridge response"}


def result_response(request_id, result):
    return {"jsonrpc": "2.0", "id": request_id, "result": result}


def handle(request):
    request_id = request.get("id")
    method = request.get("method")

    if method == "initialize":
        return result_response(request_id, {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "ulre-mcp-server", "version": "1.0.0"},
        })
    if method == "notifications/initialized":
        return None
    if method == "tools/list":
        return result_response(request_id, {"tools": TOOLS})
    if method == "tools/call":
        params = request.get("params", {})
        tool_name = params.get("name")
        tool_names = {tool["name"] for tool in TOOLS}
        if tool_name not in tool_names:
            return result_response(request_id, {
                "isError": True,
                "content": [{"type": "text", "text": "Unknown tool: " + str(tool_name)}],
            })
        arguments = params.get("arguments", {})

        try:
            if tool_name == "compare_captures":
                value = capture_manifest.compare_captures(
                    arguments["metadata_path_a"], arguments["metadata_path_b"],
                    arguments.get("threshold", 0.01),
                    arguments.get("heatmap_dir", ".ulre-analysis"),
                    arguments.get("color_space_override"),
                    arguments.get("allow_unverified_context", False))
            elif tool_name == "analyze_capture":
                value = capture_manifest.analyze_capture(
                    arguments["metadata_path"], arguments["operation"],
                    arguments.get("color_space_override"), arguments.get("reversed_z_override"),
                    arguments.get("sample_step", 16),
                    arguments.get("allow_unverified_depth_producers", False),
                    arguments.get("mask_metadata_path"),
                    arguments.get("depth_metadata_path"))
            elif tool_name == "analyze_shadow_semantics":
                value = capture_manifest.analyze_shadow_semantics(
                    arguments["metadata_path"], arguments.get("is_shadow_map_depth", False))
            elif tool_name == "analyze_luminance":
                value = analyzer.analyze_luminance(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"), arguments["color_space"],
                    arguments.get("row_pitch"), arguments.get("mask_alpha", False))
            elif tool_name == "analyze_normals":
                value = analyzer.analyze_normals(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"), arguments.get("row_pitch"),
                    arguments.get("mask_alpha", False))
            elif tool_name == "analyze_surface":
                value = analyzer.analyze_surface(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"), arguments.get("row_pitch"),
                    arguments.get("mask_alpha", False))
            elif tool_name == "analyze_depth":
                value = analyzer.analyze_depth(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("reversed_z", False), arguments.get("clear_depth"),
                    arguments.get("row_pitch"))
            elif tool_name == "reconstruct_world_pos":
                value = analyzer.reconstruct_world_pos(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments["inverse_vp"], arguments.get("reversed_z", False),
                    arguments.get("sample_step", 16), arguments.get("clear_depth"),
                    arguments.get("viewport"), arguments.get("row_pitch"))
            elif tool_name == "compare_frames":
                value = analyzer.compare_frames(
                    arguments["path_a"], arguments["path_b"],
                    arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"),
                    arguments.get("threshold", 0.01),
                    row_pitch_a=arguments.get("row_pitch_a"),
                    row_pitch_b=arguments.get("row_pitch_b"),
                    mask_alpha=arguments.get("mask_alpha", False),
                    peak_value=arguments.get("peak_value", 1.0))
            else:
                value = call_engine(tool_name, arguments)
        except (KeyError, ValueError, TypeError, OSError, OverflowError) as exc:
            value = {"valid": False, "error": str(exc)}

        return result_response(request_id, {
            "content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False)}],
            "isError": value.get("valid") is False,
        })
    if method == "ping":
        return result_response(request_id, {})

    return {"jsonrpc": "2.0", "id": request_id, "error": {
        "code": -32601,
        "message": "Method not found",
    }}


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
            response = handle(request)
        except Exception as exc:
            response = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32603, "message": str(exc)},
            }
        if response is not None:
            sys.stdout.write(json.dumps(response, ensure_ascii=False) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
