// @ulre begin
// @ulre name material_source_pbr_advanced
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
// @ulre texture_reference emissive Fragment optional fallback
// @ulre texture_reference orm Fragment optional fallback
// @ulre sampler_reference Trilinear
// @ulre sampler_reference Linear
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// Filament 高级 PBR 材质源：与 material_source_pbr_surface 同一套基色/贴图采样，
// 但读取 64B 的 PBRAdvancedSurfaceRow，把清漆/绒面/各向异性/次表面参数一并交给
// SurfaceOutput。纹理槽与 StandardOpaque 逐字节相同（material_domain_slots.toml
// 的 ref_slots 别名），差异只在 payload 行宽（64 vs 32）。

#ifndef PBR_ADVANCED_SURFACE_SOURCE_GLSL
#define PBR_ADVANCED_SURFACE_SOURCE_GLSL

#include "module/interface/material_source_interface.glsl"
#include "module/descriptor/bindless_textures.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput source_input)
{
    PBRAdvancedSurfaceRow material_data = MTL_ROW(source_input.dataIndex);

    MaterialSourceOutput material_output;
    material_output.baseColor = material_data.base_color.rgb;
    material_output.metallic = clamp(material_data.metallic, 0.0, 1.0);
    material_output.roughness = clamp(material_data.roughness, 0.04, 1.0);
    material_output.fresnel = clamp(material_data.fresnel, 0.0, 1.0);
    material_output.normalScale = material_data.normal_scale;
    material_output.ao = 1.0;
    material_output.emissive = vec3(0.0);
    material_output.alpha = 1.0;

    // 高级字段（Filament 语义，作者侧已按 0..1 归一）
    material_output.clearCoat = clamp(material_data.clearcoat, 0.0, 1.0);
    material_output.clearCoatRoughness =
        clamp(material_data.clearcoat_roughness, 0.0, 1.0);
    material_output.sheenRoughness = clamp(material_data.sheen_roughness, 0.0, 1.0);
    material_output.anisotropy = clamp(material_data.anisotropy, 0.0, 1.0);
    material_output.subsurfacePower = clamp(material_data.subsurface_power, 0.0, 1.0);
    material_output.thickness = material_data.thickness;
    // sheen_color 在行内是 packUnorm4x8（a 通道 = 0），解包后取 rgb。
    material_output.sheenColor = unpackUnorm4x8(material_data.sheen_color).rgb;

    const uint material_data_index = source_input.dataIndex;
    const vec2 material_uv = source_input.surface.uv0;

    // 四个 optional 槽：未绑定（句柄 0）由 SampleOptional 以 1.0 保底，
    // 与乘性/替换语义等价（不改变下游数值）。
    material_output.baseColor *=
        SampleOptional(MTL_TEX(material_data_index).tex_base_color, TrilinearSampler, material_uv, vec4(1.0)).rgb;

    // glTF ORM 打包图优先（R=AO、G=Roughness、B=Metallic）；未绑定回退独立槽。
    // 与 material_source_pbr_surface 同语义（共享 StandardOpaque 槽契约）。
    const uvec2 orm_ref = MTL_TEX(material_data_index).tex_orm;
    if (orm_ref.x != 0u)
    {
        const vec3 orm = Sample2DArray(orm_ref.x, LinearSampler, material_uv, float(orm_ref.y)).rgb;
        material_output.ao        = clamp(orm.r, 0.0, 1.0);
        material_output.roughness = clamp(material_output.roughness * orm.g, 0.04, 1.0);
        material_output.metallic  = clamp(material_output.metallic * orm.b, 0.0, 1.0);
    }
    else
    {
        material_output.roughness =
            clamp(material_output.roughness * SampleOptional(MTL_TEX(material_data_index).tex_roughness, LinearSampler, material_uv, vec4(1.0)).r, 0.04, 1.0);
        material_output.metallic =
            clamp(material_output.metallic * SampleOptional(MTL_TEX(material_data_index).tex_metallic, LinearSampler, material_uv, vec4(1.0)).r, 0.0, 1.0);
        material_output.ao =
            SampleOptional(MTL_TEX(material_data_index).tex_occlusion, LinearSampler, material_uv, vec4(1.0)).r;
    }

    // 自发光：行内无 emissiveFactor（与 StandardOpaque 同），纹理即唯一光源。
    material_output.emissive =
        SampleOptional(MTL_TEX(material_data_index).tex_emissive, TrilinearSampler, material_uv, vec4(0.0)).rgb;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    // 有效 alpha 与 StandardOpaque 同语义：行比例 × 可选 opacity_mask 图案。
    // ⚠ buffer_reference 类型不能加 const 限定（glslang 限制）。
    PBRAdvancedSurfaceRow material_data = MTL_ROW(source_input.dataIndex);

    return clamp(material_data.base_color.a, 0.0, 1.0)
         * SampleOptional(
               MTL_TEX(source_input.dataIndex).tex_opacity_mask,
               LinearSampler,
               source_input.surface.uv0,
               vec4(1.0)).r;
}

#endif // PBR_ADVANCED_SURFACE_SOURCE_GLSL
