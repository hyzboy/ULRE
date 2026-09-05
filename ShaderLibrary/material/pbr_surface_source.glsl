// @ulre begin
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl_private_data PBRSurface 0 Fragment optional fallback
// @ulre texture_layer base_color Fragment optional fallback
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
    // Arena+BDA：数据行与纹理句柄同在行结构内（BDA 指针解引用）
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

    const uint base_color_handle = material_data.tex_base_color;
    if (base_color_handle != 0u)
        material_output.baseColor *=
            Sample2D(base_color_handle, TrilinearSampler, source_input.surface.uv0).rgb;

    const uint roughness_handle = material_data.tex_roughness;
    if (roughness_handle != 0u)
    {
        const float roughness_tex =
            Sample2D(roughness_handle, LinearSampler, source_input.surface.uv0).r;
        material_output.roughness =
            clamp(material_output.roughness * roughness_tex, 0.04, 1.0);
    }

    const uint metallic_handle = material_data.tex_metallic;
    if (metallic_handle != 0u)
    {
        const float metallic_tex =
            Sample2D(metallic_handle, LinearSampler, source_input.surface.uv0).r;
        material_output.metallic =
            clamp(material_output.metallic * metallic_tex, 0.0, 1.0);
    }

    const uint occlusion_handle = material_data.tex_occlusion;
    if (occlusion_handle != 0u)
        material_output.ao =
            Sample2D(occlusion_handle, LinearSampler, source_input.surface.uv0).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    PBRSurfaceRow material_data = MTL_ROW(source_input.dataIndex);
    const uint opacity_handle = material_data.tex_opacity_mask;
    return opacity_handle == 0u
        ? 1.0
        : Sample2D(
            opacity_handle, LinearSampler, source_input.surface.uv0).r;
}

#endif // PBR_SURFACE_SOURCE_GLSL
