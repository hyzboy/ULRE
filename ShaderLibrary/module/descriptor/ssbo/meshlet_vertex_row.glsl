// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MeshletRows.h → MeshletVertexRow
//   布局：kind=ssbo bda=1 access=readonly size=4 align=4 ref_align=16
//   说明：Meshlet 局部顶点索引映射到 Geometry 顶点行。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_VERTEX_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_VERTEX_ROW_GLSL

struct MeshletVertexData
{
    uint vertex_index;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer MeshletVertexBufferRef
{
    MeshletVertexData rows[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_VERTEX_ROW_GLSL
