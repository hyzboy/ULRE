// @ulre begin
// @ulre name s1_ntb
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Vertex required
// @ulre end
// Stage 1: NTB 从独立 SSBO 读取——uint 打包数组，格式由模块解码
// 发行版格式：RG8 / RG16F / RGB10A2 / R11G11B10（模块内位解码）
// 全量（有 tangent 数据）= Normal+Tangent+Bitangent 直读
// 最小（仅 Normal）= Tangent/Bitangent cross 近似（草/地表粗精度）
// 解码函数在 common/ 共享；光照消费插值语义，不关心 NTB 来源
#ifndef S1_NTB_GLSL
#define S1_NTB_GLSL

layout(set=VERTEX_SET, binding=VERTEX_NTB_BINDING) readonly buffer VertexNTBData
{
    uint data[];
} sbo_vertex_ntb;

// 占位：格式解码——T0.2 接入时按 GeometryVertexFormat 选定的格式实现
// （RG8/RG16F 用 unpackHalf2x16/unpackUnorm2x16；RGB10A2/R11G11B10 手写位解码）
vec3 GetVertexNormal()
{
    // TODO: 格式解码
    return vec3(0.0, 0.0, 1.0);
}

#endif // S1_NTB_GLSL
