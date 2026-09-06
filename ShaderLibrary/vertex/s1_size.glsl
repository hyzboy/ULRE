// @ulre begin
// @ulre name s1_size
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Size
// @ulre ssbo VertexSize VertexSize 12 Mesh required
// @ulre end
// Stage 1: 顶点尺寸/宽度从独立 SSBO 读取（VF_V2F——vec2 直读 8B/顶点，取 .x 为宽度）
// 定义 HGL_SIZE_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_SIZE_GLSL
#define S1_SIZE_GLSL

// Arena+BDA：数据经地址行表基址直取（描述符退场）
#define sbo_vertex_size (*VertexSizeDataRef(pc_vertex_index.addr_size))
float Width;

#define HGL_SIZE_LOADER { Width = sbo_vertex_size.data[pc_vertex_index.vertex_base + VertexIndexID].x; }

#endif // S1_SIZE_GLSL
