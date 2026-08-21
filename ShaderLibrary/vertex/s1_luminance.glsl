// @ulre begin
// @ulre name s1_luminance
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Luminance
// @ulre ssbo VertexLuminance VertexLuminance 1 Vertex required
// @ulre end
// Stage 1: 顶点亮度从独立 SSBO 读取（VF_V1UN8——1B/顶点）
// 打包布局：4 个亮度字节/uint（VAB stride 1 紧凑——uint 数组按 4B 对齐读）
// 注：glslang 不支持 uint8_t 数组（8bit storage 转换失败）——uint 打包解码
// 定义 HGL_LUMINANCE_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_LUMINANCE_GLSL
#define S1_LUMINANCE_GLSL

layout(set=VERTEX_SET, binding=VERTEX_LUMINANCE_BINDING, std430) readonly buffer VertexLuminanceData
{
    uint data[];
} sbo_vertex_luminance;

float Luminance;

#define HGL_LUMINANCE_LOADER \
    { \
        const uint vidx = pc_vertex_index.vertex_base + VertexIndexID; \
        const uint packed = sbo_vertex_luminance.data[vidx >> 2]; \
        Luminance = float((packed >> ((vidx & 3u) * 8u)) & 0xFFu) / 255.0; \
    }

#endif // S1_LUMINANCE_GLSL
