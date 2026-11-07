// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/ModelTemplateRows.h → ModelPartRow
//   布局：kind=ssbo bda=1 access=readonly size=128 align=16 ref_align=16
//   说明：模型部件池行（GlobalSSBOType::ModelPartPool）：一步长 128B；local_matrix @0 / 包围盒 @64/@80 / geometry_id @96。着色器经 GlobalAddresses.addr_model_part_pool 以 BDA 读取 ⇒ 本 Ref 块仅作 struct 镜像来源。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_PART_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_PART_ROW_GLSL

struct ModelPartRowData
{
    mat4 local_matrix;
    vec4 aabb_center_radius;
    vec4 aabb_extents;
    uint geometry_id;
    uint material_id;
    uint part_flags;
    uint _pad0[5];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ModelPartRowBufferRef
{
    ModelPartRowData parts[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_PART_ROW_GLSL
