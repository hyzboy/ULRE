// @sfm:require UBO viewport
//
// position2d_ortho.glsl
// 2D pixel-space ortho policy: input XY are pixel coordinates;
// transformed to clip space via viewport.ortho_matrix.

void ApplyVertexTransform(vec3 local_pos, out vec4 world_pos, out vec4 clip_pos)
{
    world_pos = vec4(local_pos.xy, 0.0, 1.0);
    clip_pos  = viewport.ortho_matrix * vec4(local_pos.xy, 0.0, 1.0);
}
