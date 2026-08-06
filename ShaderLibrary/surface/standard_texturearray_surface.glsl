// @ulre begin
// @ulre name standard_texturearray_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses skylight_simple
// @ulre uses bindless_textures
// @ulre uses surface_interface
// @ulre end
// standard_texturearray_surface.glsl — Standard Lit Surface with Texture2DArray sampling
// S6: Texture sampling migrated to bindless (bindless_tex2darray[], binding=1 on Set 4).
// Texture semantic declarations remain in the material contract for recipe extraction,
// but actual sampling is fully bindless in this shader.
// Array layer index is stored in TextureLayerRows[iid][TEXTURE_SLOT_CUSTOM0].
// This aligns with the general texture-layer architecture (no texture_id in MI).

#include "common/surface_interface.glsl"

// Bindless 2DArray rows + bindless sampler arrays
// mtl_texture_layer_rows 行表声明由 CompileCompositorMaterial 统一生成并注入
//（GetTextureHandle 宏依赖该 buffer），不再在此处 #include 展开。
#include "common/bindless_textures.glsl"


float halfLambertDiffuse(vec3 N, vec3 L)
{
    float h = dot(N, L) * 0.5 + 0.5;
    return h * h;
}

float D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * d * d + 1e-7);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}

vec3 F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    ClearCoatSurfaceData mi = mtl.data[dataIndex];

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    // Array layer index stored in TextureLayerRows[iid][TEXTURE_SLOT_CUSTOM0].
    // Handle resolved from per-instance TextureLayerRows via GetTextureHandle().
    const uint iid = si.textureLayerID;
    float layer = float(GetTextureHandle(iid, TEXTURE_SLOT_CUSTOM0));

    vec3 albedo = mi.base_color.rgb;
    const uint base_color_handle = GetTextureHandle(iid, TEXTURE_SLOT_BASE_COLOR);
    if (base_color_handle != 0u)
        albedo *= SampleBindless2DArray(base_color_handle, si.uv0, layer).rgb;

    float metallic  = clamp(mi.metallic,  0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);
    float fresnel   = clamp(mi.fresnel,   0.0, 1.0);

    const uint normal_handle = GetTextureHandle(iid, TEXTURE_SLOT_NORMAL);
    if (normal_handle != 0u)
    {
        vec3 nm = SampleBindless2DArray(normal_handle, si.uv0, layer).xyz * 2.0 - 1.0;
        nm.y = -nm.y;
        N = normalize(N + vec3(nm.xy, 0.0) * mi.normal_scale);
    }

    const uint roughness_handle = GetTextureHandle(iid, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex = SampleBindless2DArray(roughness_handle, si.uv0, layer).r;
        roughness = clamp(roughness * roughness_tex, 0.04, 1.0);
    }

    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = roughness * roughness * roughness * roughness;
    float D      = D_GGX(NdotH, alpha2);
    float G      = G_Smith(NdotV, NdotL, roughness);
    vec3  F0     = mix(vec3(fresnel), albedo, metallic);
    vec3  F      = F_Schlick(VdotH, F0);

    vec3 kd       = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse  = kd * albedo / 3.14159265 * NdotL;
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;

    vec3 color  = (diffuse + specular) * sunColor;
    color      += skyAmbient * albedo * (1.0 - metallic) * 0.2;

    SurfaceOutput so;
    so.baseColor = color;
    so.normal    = N;
    so.metallic  = metallic;
    so.roughness = roughness;
    so.fresnel   = fresnel;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return 1.0;
}