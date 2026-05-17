#ifndef ULRE_VERTEX_POLICY_BILLBOARD_CAMERA_FACING_GLSL
#define ULRE_VERTEX_POLICY_BILLBOARD_CAMERA_FACING_GLSL

// vertex_policy/billboard_camera_facing.glsl
//
// Transform policy: camera-facing billboard (world-space size).
// The position provider supplies a 2-D local offset in [-0.5, 0.5]^2 (xy).
// The Transform matrix's column lengths give the billboard's world-space
// width and height in metres; rotation is ignored because the quad always
// faces the camera.
//
// MANIFEST: {
//   "needs_camera":   true,    // camera.billboard_right, camera.billboard_up, camera.vp
//   "needs_viewport": false,
//   "ubo": ["vert_forward_ubo.glsl"]
// }
//
// Prerequisites guaranteed by CompositorAssembler before this include:
//   - position_provider/<file>.glsl  → vec3 GetPositionLocal()  (expects 2-D offset, z ignored)
//   - compositor/vert_forward_ubo.glsl → GetTransform(), camera

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    mat4  M  = GetTransform();
    vec3  center = M[3].xyz;                    // translation only

    float sx = length(M[0].xyz);                // world-space width  (metres)
    float sy = length(M[1].xyz);                // world-space height (metres)

    vec3 wpos = center
              + (local_pos.x * sx) * camera.billboard_right
              + (local_pos.y * sy) * camera.billboard_up;

    world_pos = vec4(wpos, 1.0);
    clip_pos  = camera.vp * world_pos;
}

#endif // ULRE_VERTEX_POLICY_BILLBOARD_CAMERA_FACING_GLSL
