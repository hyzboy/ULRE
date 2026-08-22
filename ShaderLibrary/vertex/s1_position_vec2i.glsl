// @ulre begin
// @ulre name s1_position_vec2i
// @ulre kind Position
// @ulre priority 9
// @ulre provide Position
// @ulre ssbo VertexPosition VertexPosition 1 Vertex required
// @ulre end
// Stage 1: 2D 顶点位置从独立 SSBO 读取——RG16i 压缩格式（像素坐标 int16×2，4B/顶点）
// 数据：int16 打包（每顶点 1 个 uint——低 16 位 x、高 16 位 y）
// 解码：符号扩展（算术移位）+ int→float（像素坐标——s2 链 PixelToLocal 变换）
// 布局注意：每顶点恰好 4B → uint data[] 4B stride 天然匹配（同 RG16F 模式）
// 接口约定：声明全局 Position + LoadVertexData()（s2/s3 模块读 Position）
// include 顺序：本模块须最后（以 #ifdef 展开 HGL_*_LOADER 宏）
#ifndef S1_POSITION_VEC2I_GLSL
#define S1_POSITION_VEC2I_GLSL

layout(set=VERTEX_SET, binding=VERTEX_POSITION_BINDING, std430) readonly buffer VertexPositionData
{
    uint data[];
} sbo_vertex_position;

vec2 Position;

void LoadVertexData()
{
#ifdef HGL_INDEX_LOADER
    HGL_INDEX_LOADER
#endif
    // 每顶点 1 uint：低 16 位 x、高 16 位 y（int16 符号扩展——算术右移）
    const uint d = sbo_vertex_position.data[pc_vertex_index.vertex_base + VertexIndexID];
    Position = vec2(float(int(d) << 16 >> 16),
                    float(int(d) >> 16));
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
#ifdef HGL_WIDTH_LOADER
    HGL_WIDTH_LOADER
#endif
}

#endif // S1_POSITION_VEC2I_GLSL
