// @ulre begin
// @ulre name s1_position_vec3
// @ulre kind Position
// @ulre priority 10
// @ulre provide Position
// @ulre ssbo VertexPosition VertexPosition 1 Vertex required
// @ulre end
// Stage 1: 顶点位置从 SSBO 读取（MeshShader 方向——顶点输入统一为 SSBO）
// 需要 VERTEX_SET / VERTEX_POSITION_BINDING 宏（descriptor_macros.glsl 提供默认值）
// 接口约定：声明全局 Position + LoadVertexData()（s2/s3 模块读 Position）
// include 顺序：本模块须最后（以 #ifdef 展开 HGL_*_LOADER 宏）
// 注意：Vulkan gl_VertexIndex 已包含 firstVertex（BaseVertex）——
//       data[gl_VertexIndex] 即绝对顶点号（VDM 大 buffer 段偏移自动含入）
// 布局注意：顶点 Position buffer 是 VAB 格式（12B/顶点紧凑）——std430 默认
//       vec3 数组 stride 16B 会错位，必须用 scalar 布局（GL_EXT_scalar_block_layout）
#ifndef S1_POSITION_VEC3_GLSL
#define S1_POSITION_VEC3_GLSL

layout(set=VERTEX_SET, binding=VERTEX_POSITION_BINDING, std430, scalar) readonly buffer VertexPositionData
{
    vec3 data[];
} sbo_vertex_position;

vec3 Position;

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
#ifdef HGL_COLORINDEX_LOADER
    HGL_COLORINDEX_LOADER
#endif
#ifdef HGL_LUMINANCE_LOADER
    HGL_LUMINANCE_LOADER
#endif
#ifdef HGL_TRANSFORMID_LOADER
    HGL_TRANSFORMID_LOADER
#endif
#ifdef HGL_WIDTH_LOADER
    HGL_WIDTH_LOADER
#endif
}

#endif // S1_POSITION_VEC3_GLSL
