// @ulre begin
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment optional fallback
// @ulre texture_reference roughness Fragment optional fallback
// @ulre texture_reference metallic Fragment optional fallback
// @ulre texture_reference occlusion Fragment optional fallback
// @ulre texture_reference opacity_mask Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// PBR material source provider for regular 2D textures.

#ifndef PBR_SURFACE_SOURCE_GLSL
#define PBR_SURFACE_SOURCE_GLSL

#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput source_input)
{
    PBRSurfaceRow material_data = MTL_ROW(source_input.dataIndex);

    MaterialSourceOutput material_output;
    material_output.baseColor = material_data.base_color.rgb;
    material_output.metallic = clamp(material_data.metallic, 0.0, 1.0);
    material_output.roughness = clamp(material_data.roughness, 0.04, 1.0);
    material_output.fresnel = clamp(material_data.fresnel, 0.0, 1.0);
    material_output.normalScale = material_data.normal_scale;
    material_output.ao = 1.0;
    material_output.emissive = vec3(0.0);
    material_output.alpha = 1.0;

    const uint material_data_index = source_input.dataIndex;
    const vec2 material_uv = source_input.surface.uv0;

    // 四个 optional 槽：未绑定（句柄 0）由 SampleOptional 以 1.0 保底，
    // 与乘性/替换语义等价（不改变下游数值）。
    material_output.baseColor *=
        SampleOptional(MTL_TEX(material_data_index).tex_base_color, TrilinearSampler, material_uv, vec4(1.0)).rgb;
    material_output.roughness =
        clamp(material_output.roughness * SampleOptional(MTL_TEX(material_data_index).tex_roughness, LinearSampler, material_uv, vec4(1.0)).r, 0.04, 1.0);
    material_output.metallic =
        clamp(material_output.metallic * SampleOptional(MTL_TEX(material_data_index).tex_metallic, LinearSampler, material_uv, vec4(1.0)).r, 0.0, 1.0);
    material_output.ao =
        SampleOptional(MTL_TEX(material_data_index).tex_occlusion, LinearSampler, material_uv, vec4(1.0)).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    // optional 槽 opacity_mask；未绑定 → 1.0（不透明）
    return SampleOptional(
        MTL_TEX(source_input.dataIndex).tex_opacity_mask,
        LinearSampler,
        source_input.surface.uv0,
        vec4(1.0)).r;
}

#endif // PBR_SURFACE_SOURCE_GLSL
