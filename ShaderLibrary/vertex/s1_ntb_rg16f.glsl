// @ulre begin
// @ulre name s1_ntb_rg16f
// @ulre kind Utility
// @ulre priority 10
// @ulre provide Normal
// @ulre ssbo VertexNTB VertexNTB 3 Vertex required
// @ulre end
// Stage 1: NTB 从独立 SSBO 读取——RG16F 压缩格式（发行版）
// 数据：Normal 存 xy 半浮点（R16G16_SFLOAT——4B/顶点），z 由 shader 重建
// 解码：uint 打包 → unpackHalf2x16 → xy → z = sqrt(1 - |xy|²) → normalize
// 优势：4B/顶点 vs 开发格式 12B——带宽 3 倍节省；uint 4B 对齐（无布局问题）
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
        const vec2 nxy = unpackHalf2x16(sbo_vertex_ntb.data[gl_VertexIndex]); \
        const float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy))); \
        Normal = normalize(vec3(nxy, nz)); \
    } while(false);

#endif // S1_NTB_RG16F_GLSL
