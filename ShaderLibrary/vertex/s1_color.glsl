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

#ifdef ULRE_MATERIAL_ARENA_BDA
// Arena+BDA：数据经地址行表基址直取（描述符退场）
#ifndef S1_VertexColorDataREF_GUARD
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexColorDataRef
#define S1_VertexColorDataREF_GUARD
#endif

#define sbo_vertex_color VertexColorDataRef(pc_vertex_index.addr_color)
#else
layout(set=VERTEX_SET, binding=VERTEX_COLOR_BINDING, std430, scalar) readonly buffer VertexColorData
{
    vec4 data[];
} sbo_vertex_color;
#endif

vec4 Color;

#define HGL_COLOR_LOADER { Color = sbo_vertex_color.data[pc_vertex_index.vertex_base + VertexIndexID]; }

#endif // S1_COLOR_GLSL
