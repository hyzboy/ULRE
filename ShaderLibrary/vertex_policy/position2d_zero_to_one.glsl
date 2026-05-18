// @sfm:no-require
//
// position2d_zero_to_one.glsl
// 2D linear-remap policy: input XY in [0,1]; remapped to NDC.
// No UBO required.

void ApplyVertexTransform(vec3 local_pos, out vec4 world_pos, out vec4 clip_pos)
{
    world_pos = vec4(0.0, 0.0, 0.0, 0.0);
    clip_pos  = vec4(local_pos.xy * 2.0 - 1.0, 0.0, 1.0);
}
