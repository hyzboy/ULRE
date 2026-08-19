// @ulre begin
// @ulre name s1_uv_rg16f
// @ulre kind Utility
// @ulre priority 8
// @ulre provide UV0
// @ulre ssbo VertexUV VertexUV 2 Vertex required
// @ulre end
// Stage 1: UV 从独立 SSBO 读取——RG16F 压缩格式（发行版——half×2，4B/顶点）
// 数据：UV 半浮点打包（每顶点 1 个 uint——低 16 位 u、高 16 位 v）
// 解码：unpackHalf2x16（GLSL 内置——精确还原 half）
// 布局注意：每顶点恰好 4B → uint data[] 4B stride 天然匹配（同 RG16F 法线模式）
#ifndef S1_UV_RG16F_GLSL
#define S1_UV_RG16F_GLSL

layout(set=VERTEX_SET, binding=VERTEX_UV_BINDING, std430) readonly buffer VertexUVData
{
    uint data[];
} sbo_vertex_uv;

vec2 TexCoord;

#define HGL_UV_LOADER { TexCoord = unpackHalf2x16(sbo_vertex_uv.data[pc_vertex_index.vertex_base + VertexIndexID]); }

#endif // S1_UV_RG16F_GLSL
