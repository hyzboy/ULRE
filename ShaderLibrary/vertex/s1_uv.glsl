// @ulre begin
// @ulre name s1_uv
// @ulre kind Utility
// @ulre priority 0
// @ulre provide UV0
// @ulre end
// Stage 1: UV 从独立 SSBO 读取——声明全局 TexCoord（主代码 fragUV0 = TexCoord）
// 定义 HGL_UV_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_UV_GLSL
#define S1_UV_GLSL

// BDA：UV 基址由 MeshDrawParams 行 addr_uv 携带（VertexUVRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_uv VertexUVRef(draw_params.addr_uv)

vec2 TexCoord;

#define HGL_UV_LOADER { TexCoord = sbo_vertex_uv.data[draw_params.vertex_base + VertexIndexID]; }

#endif // S1_UV_GLSL
