// @sfm:no-require
//
// position2d_ndc.glsl
// 2D passthrough policy: input XY coordinates are already in NDC space [-1, 1].
// No UBO required.

void ApplyVertexTransform(vec3 local_pos, out vec4 world_pos, out vec4 clip_pos)
{
    world_pos = vec4(0.0, 0.0, 0.0, 0.0);
    clip_pos  = vec4(local_pos.xy, 0.0, 1.0);
}
