// @ulre begin
// @ulre name s1_uv
// @ulre kind Utility
// @ulre priority 0
// @ulre provide UV0
// @ulre ssbo VertexUV VertexUV 2 Vertex required
// @ulre end
// Stage 1: UV 从独立 SSBO 读取
#ifndef S1_UV_GLSL
#define S1_UV_GLSL

layout(set=VERTEX_SET, binding=VERTEX_UV_BINDING) readonly buffer VertexUVData
{
    vec2 data[];
} sbo_vertex_uv;

vec2 GetVertexUV()
{
    return sbo_vertex_uv.data[gl_VertexIndex];
}

#endif // S1_UV_GLSL
