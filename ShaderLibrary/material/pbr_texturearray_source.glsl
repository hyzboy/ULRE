// @ulre begin
// @ulre name pbr_texturearray_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl_private_data PBRSurface 0 Fragment optional fallback
// @ulre texture_layer custom0 Fragment required fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// PBR material source provider for Texture2DArray textures.

#ifndef PBR_TEXTUREARRAY_SOURCE_GLSL
#define PBR_TEXTUREARRAY_SOURCE_GLSL

#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput source_input)
{
    const PBRSurfaceData material_data = MTL_DATA.data[source_input.dataIndex];
    // CUSTOM0 槽在此材质中存的是 2DArray 的 layer 值（浮点），而非纹理 handle。
    const float layer = float(mtl_texture_layer_rows.data[source_input.dataIndex].custom0);

    MaterialSourceOutput material_output;
    material_output.baseColor = material_data.base_color.rgb;
    material_output.metallic = clamp(material_data.metallic, 0.0, 1.0);
    material_output.roughness = clamp(material_data.roughness, 0.04, 1.0);
    material_output.fresnel = clamp(material_data.fresnel, 0.0, 1.0);
    material_output.normalScale = material_data.normal_scale;
    material_output.ao = 1.0;
    material_output.emissive = vec3(0.0);
    material_output.alpha = 1.0;

    const uint base_color_handle = mtl_texture_layer_rows.data[source_input.dataIndex].base_color;
    if (base_color_handle != 0u)
        material_output.baseColor *=
            Sample2DArray(base_color_handle, TrilinearSampler, source_input.surface.uv0, layer).rgb;

    const uint roughness_handle = mtl_texture_layer_rows.data[source_input.dataIndex].roughness;
    if (roughness_handle != 0u)
    {
        const float roughness_tex =
            Sample2DArray(roughness_handle, LinearSampler, source_input.surface.uv0, layer).r;
        material_output.roughness =
            clamp(material_output.roughness * roughness_tex, 0.04, 1.0);
    }

    const uint metallic_handle = mtl_texture_layer_rows.data[source_input.dataIndex].metallic;
    if (metallic_handle != 0u)
    {
        const float metallic_tex =
            Sample2DArray(metallic_handle, LinearSampler, source_input.surface.uv0, layer).r;
        material_output.metallic =
            clamp(material_output.metallic * metallic_tex, 0.0, 1.0);
    }

    const uint occlusion_handle = mtl_texture_layer_rows.data[source_input.dataIndex].occlusion;
    if (occlusion_handle != 0u)
        material_output.ao =
            Sample2DArray(occlusion_handle, LinearSampler, source_input.surface.uv0, layer).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    const uint opacity_handle =
        mtl_texture_layer_rows.data[source_input.dataIndex].opacity_mask;
    if (opacity_handle == 0u)
        return 1.0;

    const float layer = float(
        mtl_texture_layer_rows.data[source_input.dataIndex].custom0);
    return Sample2DArray(
        opacity_handle,
        LinearSampler,
        source_input.surface.uv0,
        layer).r;
}

#endif // PBR_TEXTUREARRAY_SOURCE_GLSL
