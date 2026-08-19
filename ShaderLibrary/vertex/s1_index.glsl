// @ulre begin
// @ulre name s1_index
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexIndex VertexIndex 8 Vertex required
// @ulre end
// Stage 1: 顶点索引从独立 SSBO 读取（非索引绘制——gl_VertexIndex 线性遍历索引表）。
// 引擎统一 uint32 索引（废弃 U8/U16）——IBO 数据按 uint32 数组直读。
// 段偏移 index_base/vertex_base 由 push constant 传入。

#ifndef HGL_INDEX_LOADER_DEFINED
#define HGL_INDEX_LOADER_DEFINED

layout(set=VERTEX_SET, binding=VERTEX_INDEX_BINDING, std430, scalar) readonly buffer VertexIndexData
{
    uint data[];
} sbo_index;

layout(push_constant) uniform PC_VertexIndex
{
    uint index_base;
    uint vertex_base;
} pc_vertex_index;

uint VertexIndexID;    // 解码后的顶点索引（顶点数据模块按此索引读）

#define HGL_INDEX_LOADER \
    VertexIndexID = sbo_index.data[pc_vertex_index.index_base + gl_VertexIndex];

#endif//HGL_INDEX_LOADER_DEFINED
