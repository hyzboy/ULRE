// @ulre begin
// @ulre name stream_position_vec2
// @ulre kind Position
// @ulre priority 8
// @ulre uses stream_defaults
// @ulre provide Position
// @ulre end
// Stage 1: 顶点位置从 SSBO 读取（2D float——VF_V2F，8B/顶点）
// scalar 布局 vec2 数组 stride 8B 紧凑（VAB 格式直读）
#ifndef S1_POSITION_VEC2_GLSL
#define S1_POSITION_VEC2_GLSL

// BDA：位置基址由 MeshDrawParams 行 addr_position 携带（VertexPositionV2Ref 由
// MeshShaderVertexAdapter 集中声明）——vec2 8B stride 与 VAB 布局一致，读法不变
#define sbo_vertex_position VertexPositionV2Ref(draw_params.addr_position)

// 缺流空实现必须先于 LoadVertexData 可见
#include "module/vertex/stream_defaults.glsl"

vec2 Position;

void LoadVertexData()
{
    Position = sbo_vertex_position.data[draw_params.vertex_base + VertexIndexID];
    LoadUV();
    LoadNTB();
    LoadColor();
    LoadLuminance();
    LoadTransformID();
    LoadSize();
}

#endif // S1_POSITION_VEC2_GLSL
