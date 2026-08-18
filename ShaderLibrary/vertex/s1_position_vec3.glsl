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
#ifndef S1_POSITION_VEC3_GLSL
#define S1_POSITION_VEC3_GLSL

layout(set=VERTEX_SET, binding=VERTEX_POSITION_BINDING) readonly buffer VertexPositionData
{
    vec3 data[];
} sbo_vertex_position;

vec3 Position;

void LoadVertexData()
{
    Position = sbo_vertex_position.data[gl_VertexIndex];
#ifdef HGL_UV_LOADER
    HGL_UV_LOADER
#endif
#ifdef HGL_NTB_LOADER
    HGL_NTB_LOADER
#endif
#ifdef HGL_JOINT_LOADER
    HGL_JOINT_LOADER
#endif
}

#endif // S1_POSITION_VEC3_GLSL
