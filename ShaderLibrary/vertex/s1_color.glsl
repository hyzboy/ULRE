// @ulre begin
// @ulre name s1_color
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Color
// @ulre ssbo VertexColor VertexColor 4 Mesh required
// @ulre end
// Stage 1: 顶点色从独立 SSBO 读取（VF_V4F——vec4 直读 16B/顶点）
// 定义 HGL_COLOR_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_COLOR_GLSL
#define S1_COLOR_GLSL

// BDA：顶点色基址由 MeshDrawParams 行 addr_color 携带（VertexColorRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_color VertexColorRef(pc_vertex_index.addr_color)

vec4 Color;

#define HGL_COLOR_LOADER { Color = sbo_vertex_color.data[pc_vertex_index.vertex_base + VertexIndexID]; }

#endif // S1_COLOR_GLSL
