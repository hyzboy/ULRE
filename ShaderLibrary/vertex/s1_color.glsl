// @ulre begin
// @ulre name s1_color
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Color
// @ulre end
// Stage 1: 顶点色从独立 SSBO 读取（VF_V4F——vec4 直读 16B/顶点）
// 提供 LoadColor()，由 s1_position_* 的 LoadVertexData 调用
#ifndef S1_COLOR_GLSL
#define S1_COLOR_GLSL

// BDA：顶点色基址由 MeshDrawParams 行 addr_color 携带（VertexColorRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_color VertexColorRef(draw_params.addr_color)

vec4 Color;

#define S1_STREAM_COLOR 1

void LoadColor()
{
    Color = sbo_vertex_color.data[draw_params.vertex_base + VertexIndexID];
}

// 语义取值口：形状主着色器只写 `fragVertexColor[vid] = GetVertexColor();`，
// 由本模块决定「顶点色从哪来」。调色板索引版（s1_palette_index）提供同名函数，
// 因此两种顶点色来源共用同一份网格阶段主着色器。
vec4 GetVertexColor() { return Color; }

#endif // S1_COLOR_GLSL
