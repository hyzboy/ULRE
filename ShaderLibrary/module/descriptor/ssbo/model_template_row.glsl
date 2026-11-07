// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/ModelTemplateRows.h → ModelTemplateRow
//   布局：kind=ssbo bda=1 access=readonly size=128 align=16 ref_align=16
//   说明：模型模板池行（GlobalSSBOType::ModelTemplatePool）：一步长 128B；行 0 = 非法模板哨兵。着色器经 GlobalAddresses.addr_model_template_pool 以 BDA 读取 ⇒ 本 Ref 块仅作 struct 镜像来源。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_TEMPLATE_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_TEMPLATE_ROW_GLSL

struct ModelTemplateRowData
{
    uint part_offset;
    uint part_count;
    uint lod_proxy_template_ids[3];
    float lod_screen_ratios[3];
    uint shadow_proxy_template_id;
    uint flags;
    uint part_bucket_count;
    uint _pad0[1];
    vec4 local_bound_center_radius;
    vec4 local_bound_extents;
    uint _pad1[12];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ModelTemplateRowBufferRef
{
    ModelTemplateRowData templates[];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_MODEL_TEMPLATE_ROW_GLSL
