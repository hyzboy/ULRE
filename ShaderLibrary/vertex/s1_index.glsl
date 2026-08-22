// @ulre begin
// @ulre name s1_index
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexIndex VertexIndex 1 Vertex optional
// @ulre end
// Stage 1: 顶点索引按绘制类型解析（pc_vertex_index.is_indexed 区分）：
//   索引绘制（is_indexed=1）  ：gl_VertexIndex 是索引 buffer 下标——sbo_index 查表得索引值
//   非索引绘制（is_indexed=0）：gl_VertexIndex 即顶点序号——直通
// sbo_index 为 optional：非索引几何无 IBO 不绑定（descriptorBindingPartiallyBound
// 下 if 分支不执行——安全）；索引几何绑定 IBO buffer。

#ifndef HGL_INDEX_LOADER_DEFINED
#define HGL_INDEX_LOADER_DEFINED

layout(push_constant) uniform PC_VertexIndex
{
    uint index_base;
    uint vertex_base;
    uint is_indexed;
    uint total_vertices;    // mesh shader 边界检查（每帧顶点总数）；VS 材质不用（恒 0）
    float viewport_height;  // mesh shader 线宽像素换算（viewport 像素高度）；VS 材质不用（恒 0）
    uint first_instance;    // mesh shader 实例基址（= Draw 的 firstInstance——l2w_index_rows 按整批 item 序号写，实例索引 = first_instance + gl_WorkGroupID.y）
} pc_vertex_index;

layout(set=VERTEX_SET, binding=VERTEX_INDEX_BINDING, std430) readonly buffer VertexIndexData
{
    uint data[];
} sbo_vertex_index;

uint VertexIndexID;    // 顶点索引（最终顶点序号）

#define HGL_INDEX_LOADER \
    VertexIndexID = (pc_vertex_index.is_indexed != 0u) \
        ? sbo_vertex_index.data[pc_vertex_index.index_base + uint(gl_VertexIndex)] \
        : uint(gl_VertexIndex);

#endif//HGL_INDEX_LOADER_DEFINED
