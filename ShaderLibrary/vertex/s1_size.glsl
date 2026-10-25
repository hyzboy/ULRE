// @ulre begin
// @ulre name s1_size
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Size
// @ulre end
// Stage 1: 顶点尺寸/宽度从独立 SSBO 读取（VF_V2F——vec2 直读 8B/顶点，取 .x 为宽度）
// 定义 HGL_SIZE_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_SIZE_GLSL
#define S1_SIZE_GLSL

// BDA：尺寸基址由 MeshDrawParams 行 addr_size 携带（VertexSizeRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_size VertexSizeRef(draw_params.addr_size)

float Width;

#define HGL_SIZE_LOADER { Width = sbo_vertex_size.data[draw_params.vertex_base + VertexIndexID].x; }

// 尺寸访问器：LineQuad 主体只认这一个口（.x = 满宽/最粗，.y = 最细阈值），
// 不直接认 `Width` 全局 —— 形状 master 里的主体因此不需要知道 Size 从哪来。
vec2 GetLineSize()
{
    return sbo_vertex_size.data[draw_params.vertex_base + VertexIndexID];
}

#endif // S1_SIZE_GLSL
