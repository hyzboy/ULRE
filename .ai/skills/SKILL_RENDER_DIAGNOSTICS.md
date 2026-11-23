# Render Diagnostics Workflow

## Purpose

Diagnose ULRE Vulkan rendering failures through engine-embedded telemetry instead of screenshots or third-party frame captures.

## Prerequisites

- Debug channel `debugbridge` is enabled in `ULRE_DEBUG.toml`.
- ULRE has created a Vulkan device so `DebugBridgeServer` can start.
- The MCP server is launched with `python tools/ulre-mcp-server/mcp_server.py`.

## Diagnosis order

`get_render_health` has no complete production alert coverage: an empty ledger
is reported as unavailable, not healthy. `get_pass_metrics` returns synchronized
frame snapshots and pass times but not all stored Vulkan statistics. The
main-scene primitive pipeline inspector is frame/world scoped; fast-path replay
and bounded entity-sample coverage are explicit. See
`doc/render-introspection-handoff.md` for current limitations.

1. Call `get_render_health`.
2. Classify alerts:
   - `Camera`: viewport/projection/Z convention mismatch.
   - `TextureManager`: missing asset or fallback binding.
   - `Pipeline`: shader or pipeline creation failure.
3. Call `get_pass_metrics` and rank passes by `gpu_time_ms`.
   Call `inspect_render_pipeline` to inspect its `world_id`, `frame_serial`,
   stage `coverage`, and metric units. Use `trace_entity_render_state` with the
   same selectors; `Unknown` means the entity was not covered, not that it rendered.
   `audit_cache_invariants` is tri-state; only a complete evaluation can return PASS.
4. `dump_render_target` and `dump_buffer` are deprecated compatibility tools. They return `deprecated_dump_api`; use `list_capture_targets` + `capture_frame` or `list_capture_buffers` + `capture_buffer` instead.
5. Call `list_capture_targets`, then `capture_frame` with an available swapchain, Debug View or explicitly opted-in managed offscreen `target_id` and a supported `attachment`. Poll `get_capture_status` using the returned `request_id`; release terminal status with `release_capture`. Queued requests expire after 30 seconds. Use `list_capture_buffers` and `capture_buffer` for registered GPU buffers; DrawCount `frame_id` is a dispatch sequence, while `source_world_id/source_frame_serial` identify its source world frame. This survivor count is not actual GPU draw execution.
   Use `color_depth` when both attachments must belong to the same submitted frame.
6. Use `analyze_capture` with the returned `metadata_path` for luminance, R32F depth, normals or surface. Debug View color has a binary alpha mask; swapchain statistics include background. Unknown color space requires an explicit override.
7. For masked world-position bounds, request `color_depth` on the Debug View target and pass its depth `metadata_path` plus the same result's color `metadata_path` as `mask_metadata_path`. The pair must match group, frame, target, camera and viewport. Swapchain `clear_depth` is recorded from the actual full clear when known; otherwise it is null and depth analysis remains unavailable. Unmasked reconstruction still requires `allow_unverified_depth_producers`.
8. `compare_captures` verifies compatible image layout and context when available. Strict captures require viewport/camera provenance. Ordinary offscreen captures may use explicit `allow_pixel_only_unverified: true` with `allow_unverified_context: true`; this reports scene/device/camera/viewport as unverified and is not a baseline verdict.
9. Conclude with a reproducible root cause and the exact engine subsystem/file to change.

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

1. Call `set_debug_view` to request `world_normal`, `shading_normal`, `base_color`, `surface`, `emissive`, `uv0`, or the optional `world_position` RGBA32F view.
2. Poll `get_debug_view` until `mode` equals the request; an `error` means the pass did not succeed. Capture its `target_id`, then set `off` when finished. The process-wide status is for the most recently handled world, not a world selector.
3. `get_camera_matrices` returns the latest snapshot, not necessarily the capture's camera or frame; do not use it to reconstruct an older capture.
4. Prefer `analyze_capture` with the capture's `metadata_path` so format, row pitch,
   color space, frame/camera context and Debug View alpha mask come from the manifest:
   - `analyze_luminance` for exposure, dead pixels, and NaN/Inf;
   - `analyze_normals` for length error, zero normals, and Z-axis distribution;
   - `analyze_surface` for roughness, metallic, and AO distributions.
5. If the raw capture is a depth RT:
   - `analyze_depth` for cleared/invalid coverage and depth histogram;
   - `reconstruct_world_pos` for sampled world-position bounds.
6. Call `compare_captures` for repeated captures or shader regression. It requires compatible
   formats, dimensions, viewport and camera; because scene identity is missing and device
   identity is not checked, set `allow_unverified_context: true` only when that limitation is acceptable.
   - MAE/RMSE/PSNR are computed over all RGB channel samples in the intersection of valid masks;
     `channel_metrics` also reports each channel separately.
   - `threshold` is in normalized encoded-channel units; a pixel changes only when any RGB
     channel's absolute error is strictly greater than it.
   - SSIM uses non-overlapping 8×8 windows (partial edge windows allowed), population variance,
     per-channel SSIM averaged equally, then weighted by each window's valid pixel count.
     It is an encoded-channel similarity measure, not a perceptual metric.
   - `changed_regions` uses 4-connectivity, analyzes up to 262,144 pixels and returns at most
     the largest 64 areas. Above the component-analysis limit, only that metric is unavailable.
   - `mask.coverage_changed_pixels` reports the symmetric difference separately; it is not
     folded into image-error metrics. Heatmap coverage changes are white (255).
   - Read `algorithm_parameters`, `input_capture_ids`, sample counts and `limitations` alongside
     scores. PSNR infinity is represented by `psnr: null` and `psnr_infinite: true`.
   - `compare_frames` is the legacy raw-file interface; provide width, height, format and row
     pitch when applicable. The PGM heatmap is evidence, not a baseline verdict.
7. For Position RT verification, select `world_position`, capture `color_depth` from that debug target in one request, then call `analyze_capture` with `operation: "world_position_rt"` and `operation: "position_crosscheck"` (the latter also takes the paired depth manifest as `depth_metadata_path`). Read the measured near/middle/far error bands; they are descriptive and have no fixed physical tolerance.
8. Call `analyze_shadow_semantics` for shadow interpretation. Mark `is_shadow_map_depth: true` only for an actual shadow-map depth capture: non-clear ratio is caster projection coverage, not screen shadow. Screen-space shadow ratio remains unavailable without a real same-frame receiver shadow factor/mask; cascade, PCF and bias assumptions are reported only when captured.

## Quantitative decision tree

- **World normal length-error ratio > 5%**: investigate the encoded channel and valid-pixel mask. The current view normalizes its output and cannot establish whether the source was normalized.
- **Zero-normal ratio elevated**: investigate missing Normal semantics or invalid values, while excluding background pixels.
- **Negative-Z/positive-Z distribution is wrong for the known surface**: investigate the encoded
  coordinate space only when the expected surface orientation is known. The statistic is not a
  front/back-facing test; that requires an actual per-pixel view direction.
- **Roughness or metallic concentrated at 0 or 1**: compare with the authored material; either endpoint can be intentional and endpoint ratios are descriptive, not failures.
- **Underexposed/overexposed ratio is isolated to a region**: inspect shadow, AO, emissive, and light parameters for that region.
- **Depth effective ratio is near 0**: inspect geometry coverage and the depth pass; the metric excludes the conventional clear value.
- **Depth effective ratio is near 1**: inspect expected scene coverage; a full-depth image is not necessarily an error.
- **MAE/RMSE rises after a code change with a low changed-ratio**: suspect anti-aliasing, one-pixel offset, or sparse hot pixels.
- **MAE/RMSE rises with high changed-ratio across geometry**: suspect camera, projection, transform, or pipeline-state drift.
- **Heatmap follows a mesh silhouette**: suspect transform, culling, or normal/camera-space math.

## World-position reconstruction

World position is reconstructed from same-frame depth plus `inverse_vp`; an opt-in RGBA32F Position RT can be captured alongside depth and cross-checked. A paired alpha mask identifies pixels produced by supported geometry. Position RT is unavailable if the device cannot create and read back the format or if camera/material conditions are unsupported.

The Vulkan RH ZO convention is mandatory:

`u = (px + 0.5 - viewport.x) / viewport.width; v = (py + 0.5 - viewport.y) / viewport.height`

`ndc = (2u - 1, 2v - 1, (depth - min_depth) / (max_depth - min_depth), 1)`

`world = inverse_vp * ndc`, then divide by `w`.

Use capture metadata, not the latest camera snapshot. Matrices are column-major; the coordinate contract is Vulkan RH, Z-up, ZO, with a positive-height viewport. Clear-depth filtering applies when no mask proves the pixel contains geometry; homogeneous `w` must be finite and nonzero. Position RT reports absolute world-space XYZ in IEEE-754 binary32 with alpha mask; current transforms do not enable camera-relative offsets. Report measured error only—no fixed physical precision claim.

Debug View accepts GPU-driven StandardOpaque triangle batches with PBRSurface rows, Normal and TexCoord, and standard- or reversed-Z active cameras. Alpha test uses the main material's opacity texture, row alpha, vertex/instance alpha and cutoff; a vertex color stream must be bound and use VF_V4F. Dither, blending, custom vertex/surface/sampler/defines and unsupported scene batches fail explicitly; non-Scene layers are excluded. On Intel Arc 140T, same-frame probes found matching alpha but differing dither thresholds between shader contexts. An experimental precise calculation changed main-frame coverage, so it is not a safe replacement; the exact compiler/driver operation responsible remains unproven. `world_normal` is the interpolated vertex normal, not necessarily a face normal; `shading_normal` includes StandardOpaque's normal map and derivative TBN. Capture metadata records their distinct sources. Base color excludes vertex color; emissive is the sampled texture; UV uses its fractional part. RGBA8 attribute values are clipped/quantized and are not HDR.

The enabled pass waits for device idle before reusing its attachments; do not use its timings as production performance measurements. Disabling releases the target and attachments, while existing manager caches retain shader/pipeline/render-pass objects until their normal teardown.

## BasicLitMeshes MCP sequence

Start `BasicLitMeshes` with `debugbridge = true` in an isolated `ULRE_DEBUG.toml`;
the default is off. The following are tool names and argument objects, not raw pipe
JSON-RPC envelopes. Do not copy IDs from another run.

1. `list_capture_targets({})`; confirm the engine is ready.
2. `set_debug_view({"mode":"world_position"})`, then poll `get_debug_view({})`
   until the requested mode and a nonzero `target_id` appear. Stop on `error`.
3. `capture_frame({"target_id": <target_id>, "attachment":"color_depth"})`.
   Poll `get_capture_status({"request_id": <request_id>})` for this exact ID until
   `succeeded`, `failed`, or `cancelled`; a timeout is not success.
4. On success, use the color and depth `metadata_path` values from `captures`:
   `analyze_capture({"metadata_path": <color_path>, "operation":"world_position_rt"})`
   and `analyze_capture({"metadata_path": <color_path>, "operation":"position_crosscheck",
   "depth_metadata_path": <depth_path>, "sample_step":8})`.
5. `release_capture({"request_id": <request_id>})`, then
   `set_debug_view({"mode":"off"})` and poll `get_debug_view({})` until the
   target is gone. Release any other completed request separately.

For two compatible color captures, `compare_captures` needs
`metadata_path_a`, `metadata_path_b`, and `allow_unverified_context: true`;
this opt-in explicitly admits that scene and device identity are not verified.
It is **not** a golden-baseline acceptance verdict. For the fixed
`RenderDiagnosticsFixture`, use `tools\ulre-mcp-server\render_regression.py`
with `create`, `update`, or `compare`; see `doc\debug-analysis-and-view-modes.md`
for commands and provenance rules. Missing coverage must not be reported as a
pass. The ordinary fixture does not exercise alpha-test scenes, and a receiver
shadow factor is unavailable. The fixture's
`--mode shadow_caster` captures its explicitly managed CSM depth map;
`--inject-fault shadow_caster` checks caster coverage, not screen shadowing.
