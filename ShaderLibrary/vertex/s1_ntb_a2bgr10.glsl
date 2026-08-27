// @ulre begin
// @ulre name s1_ntb_a2bgr10
// @ulre kind Utility
// @ulre priority 9
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Mesh required
// @ulre end
// Stage 1: NTB 压缩格式（A2BGR10UN——uint 打包 4B/顶点，模块内位解码）
// 同一 VertexNTB SSBO——仅解码实现不同（C++ 侧零感知）
// A2BGR10UN 位布局：R=bits 0-9, G=bits 10-19, B=bits 20-29, A=bits 30-31
// UNORM 分量 → SNORM 法线（-1..1）
#ifndef S1_NTB_A2BGR10_GLSL
#define S1_NTB_A2BGR10_GLSL

layout(set=VERTEX_SET, binding=VERTEX_NTB_BINDING, std430) readonly buffer VertexNTBData
{
    uint data[];
} sbo_vertex_ntb;

vec3 Normal;

#define HGL_NTB_LOADER \
    { \
        const uint packed = sbo_vertex_ntb.data[pc_vertex_index.vertex_base + VertexIndexID]; \
        Normal = vec3(float(packed & 0x3FFu) / 1023.0 * 2.0 - 1.0, \
                      float((packed >> 10) & 0x3FFu) / 1023.0 * 2.0 - 1.0, \
                      float((packed >> 20) & 0x3FFu) / 1023.0 * 2.0 - 1.0); \
    }

#endif // S1_NTB_A2BGR10_GLSL
