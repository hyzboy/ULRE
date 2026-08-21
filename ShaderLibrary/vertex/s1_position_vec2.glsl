// @ulre begin
// @ulre name s1_position_vec2
// @ulre kind Position
// @ulre priority 8
// @ulre provide Position
// @ulre ssbo VertexPosition VertexPosition 1 Vertex required
// @ulre end
// Stage 1: 顶点位置从 SSBO 读取（2D float——VF_V2F，8B/顶点）
// scalar 布局 vec2 数组 stride 8B 紧凑（VAB 格式直读）
#ifndef S1_POSITION_VEC2_GLSL
#define S1_POSITION_VEC2_GLSL

layout(set=VERTEX_SET, binding=VERTEX_POSITION_BINDING, std430, scalar) readonly buffer VertexPositionData
{
    vec2 data[];
} sbo_vertex_position;

vec2 Position;

void LoadVertexData()
{
#ifdef HGL_INDEX_LOADER
    HGL_INDEX_LOADER
#endif
    Position = sbo_vertex_position.data[pc_vertex_index.vertex_base + VertexIndexID];
#ifdef HGL_UV_LOADER
    HGL_UV_LOADER
#endif
#ifdef HGL_NTB_LOADER
    HGL_NTB_LOADER
#endif
#ifdef HGL_JOINT_LOADER
    HGL_JOINT_LOADER
#endif
#ifdef HGL_COLOR_LOADER
    HGL_COLOR_LOADER
#endif
#ifdef HGL_LUMINANCE_LOADER
    HGL_LUMINANCE_LOADER
#endif
}

#endif // S1_POSITION_VEC2_GLSL
