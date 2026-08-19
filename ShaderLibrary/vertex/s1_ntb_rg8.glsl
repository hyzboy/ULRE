// @ulre begin
// @ulre name s1_ntb_rg8
// @ulre kind Utility
// @ulre priority 8
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Vertex required
// @ulre end
// Stage 1: NTB 从独立 SSBO 读取——RG8 最小格式（发行版——草/地表等粗精度）
// 数据：法线 octahedral 编码 → uint8 量化（R8G8_UNORM 2B/顶点）
// 解码：uint 打包（低字节 p、高字节 q）→ /255 → octahedral 展开 → normalize
// 精度：8bit/分量（法线角 ~0.5°）——带宽最优（2B/顶点——RG16F 的一半）
// 布局注意：uint 4B 对齐（2 个顶点/uint——不越界——连续读取）
layout(set=VERTEX_SET, binding=VERTEX_NTB_BINDING, std430) readonly buffer VertexNTBData
{
    uint data[];
} sbo_vertex_ntb;

vec3 Normal;

// 解码：p,q ∈ [0,255] → [-1,1] → octahedral 展开（z 符号保留——阈值防 uint8 量化抖动）
// 展开阈值 -0.02：RG8 量化误差 ±0.008（|p|+|q| 抖动超过 RG16F 的 -0.005——必须更严防翻转）
#define HGL_NTB_LOADER \
    do \
    { \
        const uint d = sbo_vertex_ntb.data[(pc_vertex_index.vertex_base + VertexIndexID) >> 1u]; \
        const uint sh = ((pc_vertex_index.vertex_base + VertexIndexID) & 1u) * 16u; \
        const vec2 p = vec2(float((d >> sh) & 0xFFu), float((d >> (sh + 8u)) & 0xFFu)) * (2.0 / 255.0) - 1.0; \
        vec3 n = vec3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y)); \
        if (n.z < -0.02) \
        { \
            n.xy = (1.0 - abs(n.yx)) * sign(n.xy); \
        } \
        Normal = normalize(n); \
    } while(false);
