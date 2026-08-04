// @ulre begin
// @ulre name standard_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses skylight_simple
// @ulre uses bindless_textures
// @ulre uses descriptor_macros
// @ulre uses surface_interface
// @ulre end
// standard_surface.glsl — Standard Lit Surface
// 固定使用贴图 + 法线 + MR 的统一 PBR 路径。

#include "common/surface_interface.glsl"

// ─── Bindless 纹理 ────────────────────────────────────────────────────────────
// mtl_texture_layer_rows 行表声明由 CompileCompositorMaterial 统一生成并注入
//（GetTextureHandle 宏依赖该 buffer），不再在此处 #include 展开。
#include "common/descriptor_macros.glsl"
#include "common/bindless_textures.glsl"

// ─── Sky Light ────────────────────────────────────────────────────────────────

// ─── Helpers ─────────────────────────────────────────────────────────────────

float halfLambertDiffuse(vec3 N, vec3 L)
{
    float h = dot(N, L) * 0.5 + 0.5;
    return h * h;
}

// ─── Surface Entry ────────────────────────────────────────────────────────────

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    ClearCoatSurfaceData mi = mtl.mi[miID];

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    // ── Base color ────────────────────────────────────────────────────────────
    vec3 albedo = unpackUnorm4x8(mi.base_color).rgb;
    const uint base_color_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_BASE_COLOR);
    if (base_color_handle != 0u)
        albedo *= SampleBindless2D(base_color_handle, si.uv0).rgb;

    float metallic  = clamp(mi.metallic,  0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

    // ── Normal Map ────────────────────────────────────────────────────────────
    const uint normal_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_NORMAL);
    if (normal_handle != 0u)
    {
        vec3 nm = SampleBindless2D(normal_handle, si.uv0).xyz * 2.0 - 1.0;
        nm.y = -nm.y;
        N = normalize(N + vec3(nm.xy, 0.0) * mi.normal_scale);
    }

    // ── Roughness Map ─────────────────────────────────────────────────────────
    const uint roughness_handle = GetTextureHandle(si.textureLayerID, TEXTURE_SLOT_ROUGHNESS);
    if (roughness_handle != 0u)
    {
        const float roughness_tex = SampleBindless2D(roughness_handle, si.uv0).r;
        roughness = clamp(roughness * roughness_tex, 0.04, 1.0);
    }

    // ── Simplified Cook-Torrance PBR (no IBL, no cubemap) ────────────────────
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = roughness * roughness * roughness * roughness;
    float D      = ULRE_D_GGX(NdotH, alpha2);
    float G      = ULRE_G_Smith(NdotV, NdotL, roughness);
    vec3  F0     = mix(vec3(0.04), albedo, metallic);
    vec3  F      = ULRE_F_Schlick(VdotH, F0);

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
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}

float EvalAlpha(SurfaceInput si, uint miID)
{
    return 1.0;
}
