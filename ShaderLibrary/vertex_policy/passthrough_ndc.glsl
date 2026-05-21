// @sfm:no-require
#ifndef ULRE_VERTEX_POLICY_PASSTHROUGH_NDC_GLSL
#define ULRE_VERTEX_POLICY_PASSTHROUGH_NDC_GLSL

// vertex_policy/passthrough_ndc.glsl
//
// Transform policy: NDC passthrough — no matrix multiplication.
// For use with position providers whose GetPosition() already returns
// clip/NDC coordinates (e.g. pcg_fullscreen_triangle.glsl).
//
// world_pos.w is set to 0 to signal "world position is not applicable".
//
// MANIFEST: {
//   "needs_camera":   false,
//   "needs_viewport": false,
//   "ubo": []
// }
//
// Prerequisites guaranteed by CompositorAssembler before this include:
//   - position_provider/<file>.glsl  → vec4 GetPosition()  (NDC coords)

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    world_pos = vec4(0.0);          // w=0: world position is not applicable
    clip_pos  = vec4(local_pos, 1.0);
}

#endif // ULRE_VERTEX_POLICY_PASSTHROUGH_NDC_GLSL
