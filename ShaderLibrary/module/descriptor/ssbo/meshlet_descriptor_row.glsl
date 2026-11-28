// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MeshletRows.h → MeshletDescriptorRow
//   布局：kind=ssbo bda=1 access=readonly size=16 align=16 ref_align=16
//   说明：Meshlet 描述符：偏移均为对应全局池的行号，packed_counts 的低 8 位为顶点数、次 8 位为三角形数。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_DESCRIPTOR_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_DESCRIPTOR_ROW_GLSL

struct MeshletDescriptorData
{
    uint vertex_offset;
    uint triangle_offset;
    uint bounds_index;
    uint packed_counts;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer MeshletDescriptorBufferRef
{
    MeshletDescriptorData rows[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_DESCRIPTOR_ROW_GLSL
