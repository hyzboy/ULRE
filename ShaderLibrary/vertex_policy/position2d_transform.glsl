// @sfm:require SSBO transform_id
// @sfm:require SSBO transform_data
#ifndef ULRE_VERTEX_POLICY_POSITION2D_TRANSFORM_GLSL
#define ULRE_VERTEX_POLICY_POSITION2D_TRANSFORM_GLSL

// position2d_transform.glsl
// 2D transform policy: input XY coordinates are in local/model space.
// The per-instance LocalToWorld matrix (fetched from transform SSBO) is
// applied in the XY plane; Z is discarded and W is set to 1 for clip output.
//
// This policy supports instanced 2D rendering where each entity carries its
// own TransformComponent (rotation, translation, scale in XY).

void ApplyVertexTransform(vec3 local_pos, out vec4 world_pos, out vec4 clip_pos)
{
    mat4 l2w  = GetTransform();
    world_pos = l2w * vec4(local_pos.xy, 0.0, 1.0);
    clip_pos  = vec4(world_pos.xy, 0.0, 1.0);
}

#endif // ULRE_VERTEX_POLICY_POSITION2D_TRANSFORM_GLSL
