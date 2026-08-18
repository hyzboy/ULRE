// @ulre begin
// @ulre name s1_position_vec3
// @ulre kind Position
// @ulre priority 10
// @ulre provide Position
// @ulre ssbo VertexPosition VertexPosition 1 Vertex required
// @ulre end
// Stage 1: 顶点位置从 SSBO 读取（MeshShader 方向——顶点输入统一为 SSBO）
// 需要 VERTEX_SET / VERTEX_POSITION_BINDING 宏（descriptor_macros.glsl 提供默认值）
#ifndef S1_POSITION_VEC3_GLSL
#define S1_POSITION_VEC3_GLSL

layout(set=VERTEX_SET, binding=VERTEX_POSITION_BINDING) readonly buffer VertexPositionData
{
    vec3 data[];
} sbo_vertex_position;

vec3 GetVertexPosition()
{
    return sbo_vertex_position.data[gl_VertexIndex];
}

#endif // S1_POSITION_VEC3_GLSL
