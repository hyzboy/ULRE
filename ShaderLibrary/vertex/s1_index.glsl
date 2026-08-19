// @ulre begin
// @ulre name s1_index
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexIndex VertexIndex 8 Vertex required
// @ulre end
// Stage 1: 顶点索引从独立 SSBO 读取（非索引绘制——gl_VertexIndex 线性遍历索引表）。
// IBO 可能是 U8/U16/U32——按 per-draw 格式标志解码（push constant index_format：
// 0=U8、1=U16、2=U32）。段偏移 index_base/vertex_base 由 push constant 传入。

#ifndef HGL_INDEX_LOADER_DEFINED
#define HGL_INDEX_LOADER_DEFINED

// IBO 数据 buffer（可能 U8/U16/U32——按 pc_vertex_index.index_format 解码）
layout(set=VERTEX_SET, binding=VERTEX_INDEX_BINDING, std430, scalar) readonly buffer VertexIndexData
{
    uint data[];
} sbo_index;

layout(push_constant) uniform PC_VertexIndex
{
    uint index_base;
    uint vertex_base;
    uint index_format;
} pc_vertex_index;

uint VertexIndexID;    // 解码后的顶点索引（顶点数据模块按此索引读）

#define HGL_INDEX_LOADER \
    if (pc_vertex_index.index_format == 2u) \
    { \
        VertexIndexID = sbo_index.data[pc_vertex_index.index_base + gl_VertexIndex]; \
    } \
    else if (pc_vertex_index.index_format == 1u) \
    { \
        const uint byte_idx = (pc_vertex_index.index_base + gl_VertexIndex) * 2u; \
        VertexIndexID = (sbo_index.data[byte_idx >> 2] >> ((byte_idx & 3u) * 8u)) & 0xFFFFu; \
    } \
    else \
    { \
        const uint byte_idx = pc_vertex_index.index_base + gl_VertexIndex; \
        VertexIndexID = (sbo_index.data[byte_idx >> 2] >> ((byte_idx & 3u) * 8u)) & 0xFFu; \
    }

#endif//HGL_INDEX_LOADER_DEFINED
