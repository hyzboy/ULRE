// @ulre begin
// @ulre name stream_palette_index
// @ulre kind Utility
// @ulre priority 1
// @ulre provide Color
// @ulre end
// Stage 1: 顶点色 palette 索引（R8_UINT——1B/顶点——uint 打包 4 索引/uint 解码）
// 供 emit_vertex_color_from_palette 材质（line 等）——fragVertexColor = color_palette.color[ColorIndex]
// 提供 LoadColor()，由 s1_position_* 的 LoadVertexData 调用
#ifndef S1_PALETTE_INDEX_GLSL
#define S1_PALETTE_INDEX_GLSL

// BDA：色号基址由 MeshDrawParams 行 addr_color 携带（VertexColorPackedRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_color VertexColorPackedRef(draw_params.addr_color)

uint ColorIndex;

#define S1_STREAM_COLOR 1

void LoadColor()
{
    const uint vidx = draw_params.vertex_base + VertexIndexID;
    const uint packed = sbo_vertex_color.data[vidx >> 2];
    ColorIndex = (packed >> ((vidx & 3u) * 8u)) & 0xFFu;
}

// 语义取值口：与 s1_color.glsl 同名同签名，网格阶段主着色器因此无需区分
// 「直读顶点色」与「调色板索引」两种来源（color_palette 由 module/descriptor/ubo/scene_ubo.glsl 提供）。
vec4 GetVertexColor() { return unpackUnorm4x8(color_palette.color[ColorIndex]); }

#endif // S1_PALETTE_INDEX_GLSL
