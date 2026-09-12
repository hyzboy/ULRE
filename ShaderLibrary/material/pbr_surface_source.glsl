// @ulre begin
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl_private_data PBRSurface Fragment optional fallback
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

    const uvec2 base_color_texture =
        MTL_TEX(source_input.dataIndex).tex_base_color;
    if (base_color_texture.x != 0u)
        material_output.baseColor *=
            Sample2DArray(
                base_color_texture.x,
                TrilinearSampler,
                source_input.surface.uv0,
                float(base_color_texture.y)).rgb;

    const uvec2 roughness_texture =
        MTL_TEX(source_input.dataIndex).tex_roughness;
    if (roughness_texture.x != 0u)
    {
        const float roughness_tex =
            Sample2DArray(
                roughness_texture.x,
                LinearSampler,
                source_input.surface.uv0,
                float(roughness_texture.y)).r;
        material_output.roughness =
            clamp(material_output.roughness * roughness_tex, 0.04, 1.0);
    }

    const uvec2 metallic_texture =
        MTL_TEX(source_input.dataIndex).tex_metallic;
    if (metallic_texture.x != 0u)
    {
        const float metallic_tex =
            Sample2DArray(
                metallic_texture.x,
                LinearSampler,
                source_input.surface.uv0,
                float(metallic_texture.y)).r;
        material_output.metallic =
            clamp(material_output.metallic * metallic_tex, 0.0, 1.0);
    }

    const uvec2 occlusion_texture =
        MTL_TEX(source_input.dataIndex).tex_occlusion;
    if (occlusion_texture.x != 0u)
        material_output.ao =
            Sample2DArray(
                occlusion_texture.x,
                LinearSampler,
                source_input.surface.uv0,
                float(occlusion_texture.y)).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    PBRSurfaceRow material_data = MTL_ROW(source_input.dataIndex);
    const uvec2 opacity_texture =
        MTL_TEX(source_input.dataIndex).tex_opacity_mask;
    return opacity_texture.x == 0u
        ? 1.0
        : Sample2DArray(
            opacity_texture.x,
            LinearSampler,
            source_input.surface.uv0,
            float(opacity_texture.y)).r;
}

#endif // PBR_SURFACE_SOURCE_GLSL
