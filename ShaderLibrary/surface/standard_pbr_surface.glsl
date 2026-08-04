// @ulre begin
// @ulre name standard_pbr_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre uses skylight_simple
// @ulre uses surface_interface
// @ulre end
// standard_pbr_surface.glsl — Standard PBR surface for CMCore StandardPBRArray variant.
//
// CMCore FS compositor context provides before this include:
//   - SurfaceInput / SurfaceOutput   (common/surface_interface.glsl)
//   - ULRE_GetSkyLight*              (common/skylight_simple.glsl)
//   - GetSamplerBaseColor(mi_id, uv) -> vec4   (sampler2DArray, layer from MIT table)
//   - GetSamplerNormal(mi_id, uv)    -> vec4   (sampler2DArray, layer from MIT table)
//
// Implements the standard EvalSurface / EvalAlpha surface API.

float _pbr_halfLambert(vec3 N, vec3 L)
{
    float h = dot(N, L) * 0.5 + 0.5;
    return h * h;
}

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    vec4 baseColorSample = GetSamplerBaseColor(miID, si.uv0);
    vec3 albedo = baseColorSample.rgb;

    vec3 N = normalize(si.worldNormal);

#ifdef HAS_NORMAL
    vec4 normalSample = GetSamplerNormal(miID, si.uv0);
    vec3 nm = normalSample.xyz * 2.0 - 1.0;
    nm.y = -nm.y;
    N = normalize(N + vec3(nm.xy, 0.0));
#endif

    vec3 V = normalize(-si.worldPos);
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    float roughness = 0.5;
    float metallic  = 0.0;

    float hl  = _pbr_halfLambert(N, L);
    vec3  H   = normalize(V + L);
    float sh  = mix(256.0, 8.0, roughness);
    float spec = pow(max(dot(N, H), 0.0), sh);

    vec3 color  = albedo * hl * sunColor;
    color      += vec3(spec) * metallic * sunColor;
    color      += skyAmbient * albedo * 0.25;

    SurfaceOutput so;
    so.baseColor = color;
    so.normal    = N;
    so.metallic  = metallic;
    so.roughness = roughness;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = baseColorSample.a;
    return so;
}

float EvalAlpha(SurfaceInput si, uint miID)
{
    return GetSamplerBaseColor(miID, si.uv0).a;
}
