// @ulre begin
// @ulre name standard_texturearray_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses bindless_textures
// @ulre uses surface_interface
// @ulre uses ntb_interface
// @ulre uses lighting_interface
// @ulre end
// standard_texturearray_surface.glsl — Standard Lit Surface with Texture2DArray sampling
// S6: Texture sampling migrated to bindless (bindless_tex2darray[], binding=1 on Set 3).
// Texture semantic declarations remain in the material contract for recipe extraction,
// but actual sampling is fully bindless in this shader.
// Array layer index is stored in TextureLayerRows[iid][TEXTURE_SLOT_CUSTOM0].
// 天光/直接光/间接光与 standard_surface 共用同一套模块：
//   sky/sky_atmosphere.glsl (GetSky*) + lighting/direct_cook_torrance_pbr.glsl
//   (EvalDirectLighting) + lighting/indirect_simple_ambient.glsl (EvalIndirectLighting)。
// 本 surface 仅保留 Texture2DArray 采样差异。

#include "common/surface_interface.glsl"
#include "common/ntb_interface.glsl"
#include "common/lighting_interface.glsl"
#include "common/bindless_textures.glsl"


SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    ClearCoatSurfaceData material_data = MTL_DATA.data[dataIndex];

    vec3 albedo = material_data.base_color.rgb;
    const uint iid = si.textureLayerID;
    float layer = float(GetTextureHandle(iid, TEXTURE_SLOT_CUSTOM0));

    const uint base_color_handle = GetTextureHandle(iid, TEXTURE_SLOT_BASE_COLOR);
    if (base_color_handle != 0u)
        albedo *= SampleBindless2DArray(base_color_handle, si.uv0, layer).rgb;

    float metallic  = clamp(material_data.metallic,  0.0, 1.0);
    float roughness = clamp(material_data.roughness, 0.04, 1.0);
    float fresnel   = clamp(material_data.fresnel,   0.0, 1.0);

    vec3 N = normalize(si.worldNormal);

    const uint normal_handle = GetTextureHandle(iid, TEXTURE_SLOT_NORMAL);
    if (normal_handle != 0u)
    {
        vec3 nm = SampleBindless2DArray(normal_handle, si.uv0, layer).xyz * 2.0 - 1.0;
        nm.y = -nm.y;
        N = normalize(N + vec3(nm.xy, 0.0) * material_data.normal_scale);
    }

    const uint roughness_handle = GetTextureHandle(iid, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex = SampleBindless2DArray(roughness_handle, si.uv0, layer).r;
        roughness = clamp(roughness * roughness_tex, 0.04, 1.0);
    }

    // 无 tangent 输入（Texture2DArray 材质），由世界法线构造正交 NTB 空间
    NTBSpace ntb = BuildOrthoNTB(N);

    SurfaceOutput surf;
    surf.baseColor = albedo;
    surf.normal    = N;
    surf.metallic  = metallic;
    surf.roughness = roughness;
    surf.fresnel   = fresnel;
    surf.ao        = 1.0;
    surf.emissive  = vec3(0.0);
    surf.alpha     = 1.0;

    // 与 standard_surface 共用同一套天光 + 直接光 + 间接光模块
    vec3 lightDir   = GetSkyMainLightDir();
    vec3 lightColor = GetSkyMainLightColor();
    vec3 skyAmbient = GetSkyAmbientColor();

    vec3 directColor   = EvalDirectLighting(surf, ntb, si.viewDir, lightDir, lightColor);
    vec3 indirectColor = EvalIndirectLighting(surf, ntb, si.viewDir, skyAmbient);

    surf.baseColor = directColor + indirectColor + surf.emissive;
    return surf;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return 1.0;
}