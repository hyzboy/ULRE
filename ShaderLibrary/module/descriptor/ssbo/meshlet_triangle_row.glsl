// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MeshletRows.h → MeshletTriangleRow
//   布局：kind=ssbo bda=1 access=readonly size=4 align=4 ref_align=16
//   说明：每行打包一个三角形，低 24 位依次保存三个 8-bit Meshlet 局部顶点索引。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_TRIANGLE_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_TRIANGLE_ROW_GLSL

struct MeshletTriangleData
{
    uint packed_indices;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer MeshletTriangleBufferRef
{
    MeshletTriangleData rows[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_TRIANGLE_ROW_GLSL
