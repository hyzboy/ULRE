// @sfm:require  UBO camera
// @sfm:require  SSBO transform_id
// @sfm:require  SSBO transform_data
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

// Billboard rotation matrix for Z-up world (Vulkan right-handed):
//   local X → billboard_right (horizontal)
//   local Y → toward camera   (depth axis in Z-up world; positive = facing viewer)
//   local Z → billboard_up    (vertical, world Z maps to screen up)
// cross(up, right) gives the vector pointing toward the camera in right-handed coords.
mat3 GetBillboardRotation()
{
    vec3 toward_cam = normalize(cross(camera.billboard_up, camera.billboard_right));
    return mat3(camera.billboard_right, toward_cam, camera.billboard_up);
}

// Expose transform_mat so vert_attrib_writers.glsl can transform normals.
// For a camera-facing billboard the mesh normal is rotated by the billboard basis.
#ifdef HAS_NORMAL
#define POLICY_HAS_TRANSFORM_MAT
mat3 transform_mat = GetBillboardRotation();
#endif

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    mat4  M  = GetTransform();
    vec3  center = M[3].xyz;                    // translation only

    float sx = length(M[0].xyz);                // world-space scale X
    float sy = length(M[1].xyz);                // world-space scale Y (depth in Z-up)
    float sz = length(M[2].xyz);                // world-space scale Z (up in Z-up)

    // In Z-up world: local X=right, local Y=depth(toward camera), local Z=up
    // cross(up, right) points toward camera in Vulkan right-handed coords
    vec3 toward_cam = normalize(cross(camera.billboard_up, camera.billboard_right));

    vec3 wpos = center
              + (local_pos.x * sx) * camera.billboard_right
              + (local_pos.y * sy) * toward_cam
              + (local_pos.z * sz) * camera.billboard_up;

    world_pos = vec4(wpos, 1.0);
    clip_pos  = camera.vp * world_pos;
}

#endif // ULRE_VERTEX_POLICY_BILLBOARD_CAMERA_FACING_GLSL
