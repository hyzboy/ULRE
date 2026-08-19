// @ulre begin
// @ulre name s1_ntb_rg16f
// @ulre kind Utility
// @ulre priority 10
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Vertex required
// @ulre end
// Stage 1: NTB 从独立 SSBO 读取——RG16F 压缩格式（发行版）
// 数据：法线 octahedral 编码（2 分量半浮点——R16G16_SFLOAT 4B/顶点——完整方向含 z 符号）
// 解码：p = unpackHalf2x16 → n = (p, 1-|p.x|-|p.y|) → 若 n.z<0 折叠展开 → normalize
// 优势：4B/顶点 vs 开发格式 12B；完整方向（z 符号保留——球下半部等负 z 法线正确）
// 接口约定：声明全局 Normal + HGL_NTB_LOADER 宏（s1_position_vec3 的 LoadVertexData 展开）
// 注意：Vulkan gl_VertexIndex 已含 firstVertex——data[gl_VertexIndex] 即绝对顶点号
#ifndef S1_NTB_RG16F_GLSL
#define S1_NTB_RG16F_GLSL

layout(set=VERTEX_SET, binding=VERTEX_NTB_BINDING, std430) readonly buffer VertexNTBData
{
    uint data[];
} sbo_vertex_ntb;

vec3 Normal;

#define HGL_NTB_LOADER \
    do \
    { \
        const vec2 p = unpackHalf2x16(sbo_vertex_ntb.data[gl_VertexIndex]); \
        vec3 n = vec3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y)); \
        /* 展开阈值：half 舍入使 z≈0 抖动（±0.005）——误展开会 ±90° 翻转成黑斑 */ \
        if (n.z < -0.005) \
        { \
            n.xy = (1.0 - abs(n.yx)) * sign(n.xy); \
        } \
        Normal = normalize(n); \
    } while(false);

#endif // S1_NTB_RG16F_GLSL
