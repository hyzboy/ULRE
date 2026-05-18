#ifndef ULRE_VERTEX_POLICY_BILLBOARD_AXIS_LOCKED_GLSL
#define ULRE_VERTEX_POLICY_BILLBOARD_AXIS_LOCKED_GLSL

// vertex_policy/billboard_axis_locked.glsl
// @sfm:require  UBO camera
// @sfm:require  UBO viewport
// @sfm:require  SSBO transform_id
// @sfm:require  SSBO transform_data
//
// Transform policy: axis-locked billboard with fixed pixel size on screen.
// The position provider supplies a 2-D local offset in [-0.5, 0.5]^2 (xy).
// The Transform matrix's column lengths give the billboard's pixel width and
// height; rotation is ignored.  The final position is expressed in
// homogeneous clip-space without an intermediate world-space position.
//
// MANIFEST: {
//   "needs_camera":   true,    // camera.vp
//   "needs_viewport": true,    // viewport.canvas_resolution
//   "ubo": ["vert_forward_ubo.glsl", "common/ubo_viewport.glsl"]
// }
//
// Prerequisites guaranteed by CompositorAssembler before this include:
//   - position_provider/<file>.glsl  → vec3 GetPositionLocal()  (expects 2-D offset, z ignored)
//   - compositor/vert_forward_ubo.glsl → GetTransform(), camera
//   - common/ubo_viewport.glsl         → viewport.canvas_resolution

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    mat4 M = GetTransform();

    // Project the billboard centre (translation only, ignore scale & rotation).
    vec4 center_clip = camera.vp * vec4(M[3].xyz, 1.0);
    vec2 center_ndc  = center_clip.xy / center_clip.w;

    // Extract per-axis pixel size from Transform column lengths.
    float px = length(M[0].xyz);                        // pixel width
    float py = length(M[1].xyz);                        // pixel height
    vec2  psize_ndc = vec2(px, py) * 2.0 / vec2(viewport.canvas_resolution);

    vec2 ndc = center_ndc + local_pos.xy * psize_ndc;

    // world_pos is not meaningful in screen-space billboard math.
    world_pos = vec4(0.0);
    clip_pos  = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}

#endif // ULRE_VERTEX_POLICY_BILLBOARD_AXIS_LOCKED_GLSL
