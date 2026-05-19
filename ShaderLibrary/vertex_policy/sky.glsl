// @sfm:require UBO camera
#ifndef ULRE_VERTEX_POLICY_SKY_GLSL
#define ULRE_VERTEX_POLICY_SKY_GLSL

void ApplyVertexTransform(vec3  local_pos,
                          out vec4 world_pos,
                          out vec4 clip_pos)
{
    world_pos = vec4(local_pos, 1.0);
    clip_pos  = camera.vp * world_pos;
}

#endif // ULRE_VERTEX_POLICY_SKY_GLSL
