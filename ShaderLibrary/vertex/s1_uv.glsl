// @ulre begin
// @ulre name s1_uv
// @ulre kind Utility
// @ulre priority 0
// @ulre provide UV0
// @ulre ssbo VertexUV VertexUV 2 Vertex required
// @ulre end
// Stage 1: UV 从独立 SSBO 读取——声明全局 TexCoord（主代码 fragUV0 = TexCoord）
// 定义 HGL_UV_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_UV_GLSL
#define S1_UV_GLSL

layout(set=VERTEX_SET, binding=VERTEX_UV_BINDING) readonly buffer VertexUVData
{
    vec2 data[];
} sbo_vertex_uv;

vec2 TexCoord;

#define HGL_UV_LOADER { TexCoord = sbo_vertex_uv.data[gl_BaseVertexARB + gl_VertexIndex]; }

#endif // S1_UV_GLSL
