// @ulre begin
// @ulre name s1_position_vec2
// @ulre kind Position
// @ulre priority 8
// @ulre provide Position
// @ulre end
// Stage 1: 顶点位置从 SSBO 读取（2D float——VF_V2F，8B/顶点）
// scalar 布局 vec2 数组 stride 8B 紧凑（VAB 格式直读）
#ifndef S1_POSITION_VEC2_GLSL
#define S1_POSITION_VEC2_GLSL

// BDA：位置基址由 MeshDrawParams 行 addr_position 携带（VertexPositionV2Ref 由
// MeshShaderVertexAdapter 集中声明）——vec2 8B stride 与 VAB 布局一致，读法不变
#define sbo_vertex_position VertexPositionV2Ref(draw_params.addr_position)

vec2 Position;

void LoadVertexData()
{
    Position = sbo_vertex_position.data[draw_params.vertex_base + VertexIndexID];
#ifdef HGL_UV_LOADER
    HGL_UV_LOADER
#endif
#ifdef HGL_NTB_LOADER
    HGL_NTB_LOADER
#endif
#ifdef HGL_COLOR_LOADER
    HGL_COLOR_LOADER
#endif
#ifdef HGL_COLORINDEX_LOADER
    HGL_COLORINDEX_LOADER
#endif
#ifdef HGL_LUMINANCE_LOADER
    HGL_LUMINANCE_LOADER
#endif
#ifdef HGL_TRANSFORMID_LOADER
    HGL_TRANSFORMID_LOADER
#endif
#ifdef HGL_SIZE_LOADER
    HGL_SIZE_LOADER
#endif
}

#endif // S1_POSITION_VEC2_GLSL
