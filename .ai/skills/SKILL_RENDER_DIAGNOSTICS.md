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
4. `dump_render_target` and `dump_buffer` report the most recent C++-requested capture; they do not request one or select by name. Do not treat the result as a new capture.
5. For a swapchain capture, call `list_capture_targets`, then `capture_frame` with its `target_id` and a supported `attachment`. Poll `get_capture_status` using the returned `request_id`; release terminal status with `release_capture`. Queued requests expire after 30 seconds. Offscreen targets and buffers are not yet exposed through this API.
   Use `color_depth` when both attachments must belong to the same submitted frame.
6. Use `analyze_capture` with the returned `metadata_path` for luminance or R32F depth. Unknown color space requires an explicit override. Statistics include clear/background pixels; do not infer a geometry-only mask.
7. `analyze_capture` can sample world-position bounds from depth, but its camera is recorded at primitive collection only. Use `allow_unverified_depth_producers` only after confirming the scene has no incompatible depth writers; clear-depth pixels cannot be distinguished from geometry at that depth.
8. `compare_captures` verifies image layout and camera but lacks scene/device identity and a mask. Its opt-in comparison includes background and measures encoded channels; it is not a validated golden-baseline verdict.
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

## Advanced quantitative diagnosis

Use fixed numeric tools before looking at any image attachment.

1. Call `set_debug_view` to request `world_normal`, `base_color`, `surface`, `emissive`, or `uv0`.
2. Wait one render frame, then call `get_debug_view` and verify `mode` equals the requested mode.
3. `get_camera_matrices` returns the latest snapshot, not necessarily the capture's camera or frame; do not use it to reconstruct an older capture.
4. If the raw capture is a color/debug RT:
   - `analyze_luminance` for exposure, dead pixels, and NaN/Inf;
   - `analyze_normals` for length error, zero normals, and Z-axis distribution;
   - `analyze_surface` for roughness, metallic, and AO distributions.
5. If the raw capture is a depth RT:
   - `analyze_depth` for cleared/invalid coverage and depth histogram;
   - `reconstruct_world_pos` for sampled world-position bounds.
6. Call `compare_frames` for repeated captures or shader regression:
   - MAE is the first broad drift indicator;
   - RMSE and PSNR quantify accumulated error;
   - `changed_ratio` locates the fraction of pixels outside the threshold;
   - the PGM heatmap is evidence, not the pass/fail decision.

## Quantitative decision tree

- **World normal length-error ratio > 5%**: investigate the encoded channel and valid-pixel mask. The current view normalizes its output and cannot establish whether the source was normalized.
- **Zero-normal ratio elevated**: investigate missing Normal semantics or invalid values, while excluding background pixels.
- **Negative-Z/positive-Z distribution is wrong for the known surface**: suspect coordinate-space or front-face sign.
- **Roughness or metallic concentrated at 0 or 1**: compare with the authored material; either endpoint can be intentional.
- **Underexposed/overexposed ratio is isolated to a region**: inspect shadow, AO, emissive, and light parameters for that region.
- **Depth effective ratio is near 0**: inspect geometry coverage and the depth pass; the metric excludes the conventional clear value.
- **Depth effective ratio is near 1**: inspect expected scene coverage; a full-depth image is not necessarily an error.
- **MAE/RMSE rises after a code change with a low changed-ratio**: suspect anti-aliasing, one-pixel offset, or sparse hot pixels.
- **MAE/RMSE rises with high changed-ratio across geometry**: suspect camera, projection, transform, or pipeline-state drift.
- **Heatmap follows a mesh silhouette**: suspect transform, culling, or normal/camera-space math.

## World-position reconstruction

World position is reconstructed from depth plus `inverse_vp`; do not require an extra 64-bit WorldPosition RT.

The Vulkan RH ZO convention is mandatory:

`ndc = ((px + 0.5) / width * 2 - 1, (py + 0.5) / height * 2 - 1, depth, 1)`

`world = inverse_vp * ndc`, then divide by `w`.
