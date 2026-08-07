// @ulre begin
// @ulre name lit_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses bindless_textures
// @ulre uses descriptor_macros
// @ulre uses surface_interface
// @ulre uses ntb_interface
// @ulre end
// lit_surface.glsl — Modular Lit Surface
// 材质基本属性提取与模块化 NTB 空间解码；光照由 compositor 统一调度。

#include "common/surface_interface.glsl"
#include "common/descriptor_macros.glsl"
#include "common/bindless_textures.glsl"
#include "common/ntb_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    PBRSurfaceData material_data = MTL_DATA.data[dataIndex];

    // 1. Base Albedo & PBR Parameters
    vec3 albedo = material_data.base_color.rgb;
    const uint base_color_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_BASE_COLOR);
    if (base_color_handle != 0u)
        albedo *= SampleBindless2D(base_color_handle, si.uv0).rgb;

    float metallic  = clamp(material_data.metallic,  0.0, 1.0);
    float roughness = clamp(material_data.roughness, 0.04, 1.0);
    float fresnel   = clamp(material_data.fresnel,   0.0, 1.0);

    // Roughness Map
    const uint roughness_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex = SampleBindless2D(roughness_handle, si.uv0).r;
        roughness = clamp(roughness * roughness_tex, 0.04, 1.0);
    }

    // Metallic Map
    const uint metallic_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_METALLIC);
    if (metallic_handle != 0u)
    {
        const float metallic_tex = SampleBindless2D(metallic_handle, si.uv0).r;
        metallic = clamp(metallic * metallic_tex, 0.0, 1.0);
    }

    // 2. Normal Map & NTB Space Resolution
    const uint normal_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_NORMAL);
    NTBSpace ntb = EvalNTBSpace(si, dataIndex, material_data.normal_scale, normal_handle);

    // 3. Construct Surface Base Output Properties
    SurfaceOutput surf;
    surf.baseColor = albedo;
    surf.normal    = ntb.N;
    surf.metallic  = metallic;
    surf.roughness = roughness;
    surf.fresnel   = fresnel;
    surf.ao        = 1.0;
    surf.emissive  = vec3(0.0);
    surf.alpha     = 1.0;

    // Occlusion Map
    const uint occlusion_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_OCCLUSION);
    if (occlusion_handle != 0u)
    {
        surf.ao = SampleBindless2D(occlusion_handle, si.uv0).r;
    }

    // Lighting is scheduled by the compositor after this material output.
    return surf;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return 1.0;
}
