import json
import os
import sys

import analyzer


PIPE_NAME = r"\\.\pipe\ulre_render_debug"
MAX_PIPE_REQUEST_BYTES = 4095
MAX_PIPE_RESPONSE_BYTES = 1024 * 1024


TOOLS = [
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
        "description": "Request an engine debug attribute view.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "mode": {
                    "type": "string",
                    "enum": ["off", "world_normal", "base_color", "surface", "emissive", "uv0"],
                },
            },
            "required": ["mode"],
        },
    },
    {
        "name": "get_debug_view",
        "description": "Read the current debug attribute view.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "capture_frame",
        "description": "Request frame capture (currently unavailable).",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "analyze_luminance",
        "description": "Quantify luminance of a raw capture; supply its actual color space.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "default": "rgba8"},
            "color_space": {"type": "string", "enum": ["linear", "srgb"]},
        }, "required": ["path", "width", "height", "color_space"]},
    },
    {
        "name": "analyze_normals",
        "description": "Validate encoded world normals and their axis distribution.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "default": "rgba8"},
        }, "required": ["path", "width", "height"]},
    },
    {
        "name": "analyze_surface",
        "description": "Quantify roughness, metallic, and AO debug attributes.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "default": "rgba8"},
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
        }, "required": ["path", "width", "height"]},
    },
    {
        "name": "reconstruct_world_pos",
        "description": "Reconstruct sampled positions using the matching capture's inverse VP; never uses the latest camera implicitly.",
        "inputSchema": {"type": "object", "properties": {
            "path": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "sample_step": {"type": "integer", "minimum": 1, "default": 16},
            "inverse_vp": {"type": "array", "items": {"type": "number"},
                           "minItems": 16, "maxItems": 16},
            "reversed_z": {"type": "boolean", "default": False},
        }, "required": ["path", "width", "height", "inverse_vp"]},
    },
    {
        "name": "compare_frames",
        "description": "Compare two raw frame captures and emit MAE, RMSE, PSNR, changed ratio and heatmap.",
        "inputSchema": {"type": "object", "properties": {
            "path_a": {"type": "string"},
            "path_b": {"type": "string"},
            "width": {"type": "integer", "minimum": 1},
            "height": {"type": "integer", "minimum": 1},
            "pixel_format": {"type": "string", "default": "rgba8"},
            "threshold": {"type": "number", "minimum": 0.0, "default": 0.01},
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
        with open(PIPE_NAME, "r+b", buffering=0) as pipe:
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
            if tool_name == "analyze_luminance":
                value = analyzer.analyze_luminance(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"), arguments["color_space"])
            elif tool_name == "analyze_normals":
                value = analyzer.analyze_normals(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"))
            elif tool_name == "analyze_surface":
                value = analyzer.analyze_surface(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"))
            elif tool_name == "analyze_depth":
                value = analyzer.analyze_depth(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments.get("reversed_z", False))
            elif tool_name == "reconstruct_world_pos":
                value = analyzer.reconstruct_world_pos(
                    arguments["path"], arguments["width"], arguments["height"],
                    arguments["inverse_vp"], arguments.get("reversed_z", False),
                    arguments.get("sample_step", 16))
            elif tool_name == "compare_frames":
                value = analyzer.compare_frames(
                    arguments["path_a"], arguments["path_b"],
                    arguments["width"], arguments["height"],
                    arguments.get("pixel_format", "rgba8"),
                    arguments.get("threshold", 0.01))
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
