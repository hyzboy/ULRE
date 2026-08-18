// @ulre begin
// @ulre name s1_ntb
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Vertex required
// @ulre end
// Stage 1: NTB 从独立 SSBO 读取——内容可变（格式由模块解码）
// 当前格式：开发格式 Normal = R32G32B32_SFLOAT（VAB 12B/顶点紧凑——scalar 布局）
// 未来发行版格式：RG8/RG16F/RGB10A2/R11G11B10——uint 打包 + 模块内位解码
//   （RG16F 用 unpackHalf2x16、RG8 用 unpackUnorm2x16、RGB10A2/R11G11B10 手写位解码）
//   ——同一 VertexNTB SSBO，仅本模块的解码实现不同（C++ 侧零感知）
// 接口约定：声明全局 Normal + HGL_NTB_LOADER 宏（s1_position_vec3 的 LoadVertexData 展开）
// 注意：Vulkan gl_VertexIndex 已含 firstVertex（BaseVertex）——data[gl_VertexIndex] 即绝对顶点号
#ifndef S1_NTB_GLSL
#define S1_NTB_GLSL

layout(set=VERTEX_SET, binding=VERTEX_NTB_BINDING, std430, scalar) readonly buffer VertexNTBData
{
    vec3 data[];
} sbo_vertex_ntb;

vec3 Normal;

#define HGL_NTB_LOADER { Normal = sbo_vertex_ntb.data[gl_VertexIndex]; }

#endif // S1_NTB_GLSL
