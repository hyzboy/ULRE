// @ulre begin
// @ulre name s1_transform_id
// @ulre kind Utility
// @ulre priority 0
// @ulre ssbo VertexTransformID VertexTransformID 1 Vertex required
// @ulre end
// Stage 1: 顶点 TransformID（调色板变换索引——uint 直读）
// 定义 HGL_TRANSFORMID_LOADER 宏，由 s1_position_* 的 LoadVertexData 展开
#ifndef S1_TRANSFORM_ID_GLSL
#define S1_TRANSFORM_ID_GLSL

layout(set=VERTEX_SET, binding=VERTEX_TRANSFORMID_BINDING, std430) readonly buffer VertexTransformIDData
{
    uint data[];
} sbo_vertex_transform_id;

uint TransformID;

#define HGL_TRANSFORMID_LOADER { TransformID = sbo_vertex_transform_id.data[pc_vertex_index.vertex_base + VertexIndexID]; }

#endif // S1_TRANSFORM_ID_GLSL
