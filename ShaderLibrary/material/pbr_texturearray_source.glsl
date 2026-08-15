// @ulre begin
// @ulre name pbr_texturearray_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl PBRSurface 0 Fragment optional fallback
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
    const uint iid = source_input.surface.textureLayerID;
    const float layer = float(GetTextureHandle(iid, TEXTURE_SLOT_CUSTOM0));

    MaterialSourceOutput material_output;
    material_output.baseColor = material_data.base_color.rgb;
    material_output.metallic = clamp(material_data.metallic, 0.0, 1.0);
    material_output.roughness = clamp(material_data.roughness, 0.04, 1.0);
    material_output.fresnel = clamp(material_data.fresnel, 0.0, 1.0);
    material_output.normalScale = material_data.normal_scale;
    material_output.ao = 1.0;
    material_output.emissive = vec3(0.0);
    material_output.alpha = 1.0;

    const uint base_color_handle = GetTextureHandle(iid, TEXTURE_SLOT_BASE_COLOR);
    if (base_color_handle != 0u)
        material_output.baseColor *=
            SampleBindless2DArray(base_color_handle, source_input.surface.uv0, layer).rgb;

    const uint roughness_handle = GetTextureHandle(iid, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex =
            SampleBindless2DArray(roughness_handle, source_input.surface.uv0, layer).r;
        material_output.roughness =
            clamp(material_output.roughness * roughness_tex, 0.04, 1.0);
    }

    const uint metallic_handle = GetTextureHandle(iid, TEXTURE_SLOT_METALLIC);
    if (metallic_handle != 0u)
    {
        const float metallic_tex =
            SampleBindless2DArray(metallic_handle, source_input.surface.uv0, layer).r;
        material_output.metallic =
            clamp(material_output.metallic * metallic_tex, 0.0, 1.0);
    }

    const uint occlusion_handle = GetTextureHandle(iid, TEXTURE_SLOT_OCCLUSION);
    if (occlusion_handle != 0u)
        material_output.ao =
            SampleBindless2DArray(occlusion_handle, source_input.surface.uv0, layer).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    const uint iid = source_input.surface.textureLayerID;
    const uint opacity_handle =
        GetTextureHandle(iid, TEXTURE_SLOT_OPACITY_MASK);
    if (opacity_handle == 0u)
        return 1.0;

    const float layer = float(
        GetTextureHandle(iid, TEXTURE_SLOT_CUSTOM0));
    return SampleBindless2DArray(
        opacity_handle,
        source_input.surface.uv0,
        layer).r;
}

#endif // PBR_TEXTUREARRAY_SOURCE_GLSL
