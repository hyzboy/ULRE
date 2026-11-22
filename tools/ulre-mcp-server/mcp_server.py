import json
import os
import sys


PIPE_NAME = r"\\.\pipe\ulre_render_debug"


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
        "description": "Read the latest frame capture metadata and dump file path.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]


def call_engine(method, params=None):
    if os.name != "nt":
        return {"valid": False, "error": "ULRE named pipe is available on Windows only"}

    payload = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        payload["params"] = params
    request = json.dumps(payload)
    try:
        with open(PIPE_NAME, "r+b", buffering=0) as pipe:
            pipe.write(request.encode("utf-8"))
            response = pipe.read(4096)
    except OSError as exc:
        return {"valid": False, "error": "ULRE bridge unavailable", "detail": str(exc)}

    if not response:
        return {"valid": False, "error": "Empty ULRE bridge response"}

    envelope = json.loads(response.decode("utf-8"))
    return envelope.get("result", {"valid": False, "error": "Invalid ULRE response"})


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
        value = call_engine(tool_name, params.get("arguments", {}))
        return result_response(request_id, {
            "content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False)}],
            "isError": False,
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
