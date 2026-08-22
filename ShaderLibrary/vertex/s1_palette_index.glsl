// @ulre begin
// @ulre name s1_palette_index
// @ulre kind Utility
// @ulre priority 1
// @ulre provide Color
// @ulre ssbo VertexColor VertexColor 1 Vertex required
// @ulre end
// Stage 1: 顶点色 palette 索引（R8_UINT——1B/顶点——uint 打包 4 索引/uint 解码）
// 供 emit_vertex_color_from_palette 材质（line 等）——fragVertexColor = color_palette.color[ColorIndex]
// 定义 HGL_COLORINDEX_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_PALETTE_INDEX_GLSL
#define S1_PALETTE_INDEX_GLSL

layout(set=VERTEX_SET, binding=VERTEX_COLOR_BINDING, std430) readonly buffer VertexColorData
{
    uint data[];
} sbo_vertex_color;

uint ColorIndex;

#define HGL_COLORINDEX_LOADER \
    { \
        const uint vidx = pc_vertex_index.vertex_base + VertexIndexID; \
        const uint packed = sbo_vertex_color.data[vidx >> 2]; \
        ColorIndex = (packed >> ((vidx & 3u) * 8u)) & 0xFFu; \
    }

#endif // S1_PALETTE_INDEX_GLSL
