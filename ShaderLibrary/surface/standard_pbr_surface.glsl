// @ulre begin
// @ulre name standard_pbr_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses surface_interface
// @ulre uses ntb_interface
// @ulre uses lighting_interface
// @ulre end
// standard_pbr_surface.glsl — Standard PBR surface for CMCore StandardPBRArray variant.

#include "common/surface_interface.glsl"
#include "common/ntb_interface.glsl"
#include "common/lighting_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    vec4 baseColorSample = GetSamplerBaseColor(dataIndex, si.uv0);
    vec3 albedo = baseColorSample.rgb;

    NTBSpace ntb = BuildOrthoNTB(si.worldNormal);

#ifdef HAS_NORMAL
    vec4 normalSample = GetSamplerNormal(dataIndex, si.uv0);
    if (length(normalSample.xyz) > 0.01)
    {
        vec3 nm = normalSample.xyz * 2.0 - 1.0;
        nm.y = -nm.y;
        mat3 TBN = mat3(ntb.T, ntb.B, ntb.N);
        ntb.N = normalize(TBN * nm);
    }
#endif

    float roughness = 0.5;
    float metallic  = 0.0;

    SurfaceOutput surf;
    surf.baseColor = albedo;
    surf.normal    = ntb.N;
    surf.metallic  = metallic;
    surf.roughness = roughness;
    surf.ao        = 1.0;
    surf.emissive  = vec3(0.0);
    surf.alpha     = baseColorSample.a;

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
    return GetSamplerBaseColor(dataIndex, si.uv0).a;
}

