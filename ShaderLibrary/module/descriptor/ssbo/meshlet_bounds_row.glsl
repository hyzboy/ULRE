// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MeshletRows.h → MeshletBoundsRow
//   布局：kind=ssbo bda=1 access=readonly size=32 align=16 ref_align=16
//   说明：Meshlet bounds：center_radius 与 cone_apex，cone_axis_cutoff_packed 按 snorm4x8 解码。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_BOUNDS_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_BOUNDS_ROW_GLSL

struct MeshletBoundsData
{
    vec4 center_radius;
    float cone_apex[3];
    uint cone_axis_cutoff_packed;
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer MeshletBoundsBufferRef
{
    MeshletBoundsData rows[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MESHLET_BOUNDS_ROW_GLSL
