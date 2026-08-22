// @ulre begin
// @ulre name s1_size
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Size
// @ulre ssbo VertexSize VertexSize 12 Vertex required
// @ulre end
// Stage 1: 顶点尺寸/宽度从独立 SSBO 读取（VF_V2F——vec2 直读 8B/顶点，取 .x 为宽度）
// 定义 HGL_WIDTH_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_SIZE_GLSL
#define S1_SIZE_GLSL

layout(set=VERTEX_SET, binding=VERTEX_SIZE_BINDING, std430, scalar) readonly buffer VertexSizeData
{
    vec2 data[];
} sbo_vertex_size;

float Width;

#define HGL_WIDTH_LOADER { Width = sbo_vertex_size.data[pc_vertex_index.vertex_base + VertexIndexID].x; }

#endif // S1_SIZE_GLSL
