// @ulre begin
// @ulre name s1_index
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexIndex VertexIndex 8 Vertex required
// @ulre end
// Stage 1: 顶点索引从独立 SSBO 读取（MeshShader 方向——索引数据统一 SSBO）
// 非索引绘制（vkCmdDraw）时 gl_VertexIndex = 0..N-1 线性——索引数据按
// index_base + gl_VertexIndex 查表——顶点模块再用 index 间接读顶点数据。
// push constant（顶点 stage，8B）：index_base（IBO 段偏移 first_index）+
// vertex_base（VDM 段偏移 vertex_offset）——独立 VAB 场景均为 0。
#ifndef S1_INDEX_GLSL
#define S1_INDEX_GLSL

layout(set=VERTEX_SET, binding=VERTEX_INDEX_BINDING, std430) readonly buffer VertexIndexData
{
    uint data[];
} sbo_vertex_index;

// per-draw 段偏移（PipelineMaterialRenderer 绘制前 PushConstants）
layout(push_constant) uniform VertexIndexPC
{
    uint index_base;    // IBO 段偏移（first_index）
    uint vertex_base;   // 顶点段偏移（vertex_offset——VDM 大 buffer）
} pc_vertex_index;

uint VertexIndexID;

#define HGL_INDEX_LOADER { VertexIndexID = sbo_vertex_index.data[pc_vertex_index.index_base + gl_VertexIndex]; }

#endif // S1_INDEX_GLSL
