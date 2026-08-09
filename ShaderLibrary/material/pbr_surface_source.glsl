// @ulre begin
// @ulre name pbr_surface_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre ssbo mtl PBRSurface 0 Fragment optional fallback
// @ulre texture TextureBaseColor MaterialSampler BaseColor sampler2D Fragment optional fallback
// @ulre texture TextureMetallic MaterialSampler Metallic sampler2D Fragment optional fallback
// @ulre texture TextureRoughness MaterialSampler Roughness sampler2D Fragment optional fallback
// @ulre texture TextureOcclusion MaterialSampler Occlusion sampler2D Fragment optional fallback
// @ulre texture TextureOpacityMask MaterialSampler OpacityMask sampler2D Fragment optional fallback
// @ulre texture_layer BaseColor Fragment optional fallback
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
    const PBRSurfaceData material_data = MTL_DATA.data[source_input.dataIndex];
    const uint iid = source_input.surface.textureLayerID;

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
            SampleBindless2D(base_color_handle, source_input.surface.uv0).rgb;

    const uint roughness_handle = GetTextureHandle(iid, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex =
            SampleBindless2D(roughness_handle, source_input.surface.uv0).r;
        material_output.roughness =
            clamp(material_output.roughness * roughness_tex, 0.04, 1.0);
    }

    const uint metallic_handle = GetTextureHandle(iid, TEXTURE_SLOT_METALLIC);
    if (metallic_handle != 0u)
    {
        const float metallic_tex =
            SampleBindless2D(metallic_handle, source_input.surface.uv0).r;
        material_output.metallic =
            clamp(material_output.metallic * metallic_tex, 0.0, 1.0);
    }

    const uint occlusion_handle = GetTextureHandle(iid, TEXTURE_SLOT_OCCLUSION);
    if (occlusion_handle != 0u)
        material_output.ao =
            SampleBindless2D(occlusion_handle, source_input.surface.uv0).r;

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    const uint opacity_handle = GetTextureHandle(
        source_input.surface.textureLayerID,
        TEXTURE_SLOT_OPACITY_MASK);
    return opacity_handle == 0u
        ? 1.0
        : SampleBindless2D(
            opacity_handle, source_input.surface.uv0).r;
}

#endif // PBR_SURFACE_SOURCE_GLSL
