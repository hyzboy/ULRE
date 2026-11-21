# Render Diagnostics Workflow

## Purpose

Diagnose ULRE Vulkan rendering failures through engine-embedded telemetry instead of screenshots or third-party frame captures.

## Prerequisites

- Debug channel `debugbridge` is enabled in `ULRE_DEBUG.toml`.
- ULRE has created a Vulkan device so `DebugBridgeServer` can start.
- The MCP server is launched with `python tools/ulre-mcp-server/mcp_server.py`.

## Diagnosis order

1. Call `get_render_health`.
2. Classify alerts:
   - `Camera`: viewport/projection/Z convention mismatch.
   - `TextureManager`: missing asset or fallback binding.
   - `Pipeline`: shader or pipeline creation failure.
3. Call `get_pass_metrics` and rank passes by `gpu_time_ms`.
4. If a pass has zero useful output, call `dump_render_target` and inspect dimensions, format, and captured byte count.
5. If GPU data is suspect, call `dump_buffer` and compare row count, size, and binding state with the relevant contract header.
6. Conclude with a reproducible root cause and the exact engine subsystem/file to change.

## Camera contract

The engine contract is Vulkan RH, Z-up, ZO `[0,1]`, `m00 > 0`, and `m11 < 0` for the default positive-height viewport.

Do not mix OpenGL depth conventions or negative-Y viewport assumptions into fixes.

## Dump interpretation

- A depth capture with an unexpectedly large cleared ratio indicates low scene coverage or incorrect depth state.
- A color capture with `byte_count == 0` is a failed or unavailable capture, not a valid black frame.
- Buffer capture is only meaningful after `FrameDebugger::CollectPending()` is called by the application/engine frame loop.

## Tool failure handling

If the MCP reports `ULRE bridge unavailable`, verify:

1. `ULRE_DEBUG.toml` contains `debugbridge = true`.
2. The engine process is running and its Vulkan device is initialized.
3. No stale engine process is holding the named pipe.
