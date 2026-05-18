// @sfm:require  UBO camera
// @sfm:require  SSBO transform_id
// @sfm:require  SSBO transform_data
#ifndef ULRE_VERTEX_POLICY_MESH3D_GLSL
#define ULRE_VERTEX_POLICY_MESH3D_GLSL

// Signals to vert_attrib_writers.glsl that transform_mat is available.
#define POLICY_HAS_TRANSFORM_MAT 1

// vertex_policy/mesh3d.glsl
//
// Transform policy: standard 3-D mesh — multiply local position by the
// per-instance LocalToWorld matrix.
//
// MANIFEST: {
//   "needs_camera":   true,
//   "needs_viewport": false,
//   "ubo": ["vert_forward_ubo.glsl"]   // provides GetTransform() and camera.vp
// }
//
// Contract: implements ApplyVertexTransform() as required by the generic
// forward vertex entry (compositor/vert_forward_main.glsl).
//
// Additional export: `transform_mat` — the resolved LocalToWorld mat4.
// vert_attrib_writers.glsl relies on this to transform normals and tangents.
//
// Prerequisites guaranteed by CompositorAssembler before this include:
//   - position_provider/<file>.glsl  → vec3 GetPositionLocal()
//   - compositor/vert_forward_ubo.glsl → GetTransform(), camera.vp

// Shared LocalToWorld matrix — also consumed by vert_attrib_writers.glsl.
mat4 transform_mat;

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    transform_mat = GetTransform();
    world_pos     = transform_mat * vec4(local_pos, 1.0);
    clip_pos      = camera.vp * world_pos;
}

#endif // ULRE_VERTEX_POLICY_MESH3D_GLSL
