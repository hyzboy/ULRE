#ifndef ULRE_VERTEX_POLICY_QUAD2D_GLSL
#define ULRE_VERTEX_POLICY_QUAD2D_GLSL

// vertex_policy/quad2d.glsl
//
// Transform policy: 2-D screen-space quad.
// The position provider is expected to return coordinates already in
// normalised-device-coordinate (NDC) space, so no matrix multiplication is
// needed.  world_pos is intentionally set to w=0 to signal "not applicable".
//
// MANIFEST: {
//   "needs_camera":   false,
//   "needs_viewport": false,
//   "ubo": []
// }
//
// Prerequisites guaranteed by CompositorAssembler before this include:
//   - position_provider/<file>.glsl  → vec3 GetPositionLocal()  (expects NDC xy)

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    world_pos = vec4(0.0);          // w=0: world position is not meaningful
    clip_pos  = vec4(local_pos, 1.0);
}

#endif // ULRE_VERTEX_POLICY_QUAD2D_GLSL
