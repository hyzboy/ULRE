// @ulre begin
// @ulre name stream_transform_id
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Stage 1: 顶点 TransformID（调色板变换索引——uint32 直读）
// 引擎强制 TransformID = R32_UINT（HGL_TRANSFORM_ID_U32=1），与 l2w_index_rows
// 调色板 uint32 一致，SSBO 直读 uint 无错位（之前的 R16 错位 0x40004 已根除）。
// CPU 端 ValueType=uint32_t 写入 4B/顶点，STRIDE_BYTES 自动=4，对齐 uint data[]。
// 提供 LoadTransformID()，由 s1_position_* 的 LoadVertexData 调用
#ifndef S1_TRANSFORM_ID_GLSL
#define S1_TRANSFORM_ID_GLSL

// BDA：变换序号基址由 MeshDrawParams 行 addr_transform_id 携带
//（VertexTransformIDRef 由 MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_transform_id VertexTransformIDRef(draw_params.addr_transform_id)

uint TransformID;

#define S1_STREAM_TRANSFORM_ID 1

void LoadTransformID()
{
    TransformID = sbo_vertex_transform_id.data[draw_params.vertex_base + VertexIndexID];
}

// 选择权交给「装了什么属性」，不留给编译期分支：装了本模块就说明变换序号来自
// 顶点流（调色板逐顶点变换），`helpers/orient_world.glsl` 的 GetL2W() 据此改用
// `l2w.mats[TransformID]`，否则按实例号查 l2w_index 表。
// 此前这个宏由生成器在 C++ 侧发射（已随生成器删除），现在由模块自述 ——
// 「模块在位」与「属性在位」因此不可能再漂移。
#define HGL_L2W_FROM_VERTEX_ATTR 1

#endif // S1_TRANSFORMID_GLSL
