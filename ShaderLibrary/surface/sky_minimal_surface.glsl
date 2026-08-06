// @ulre begin
// @ulre name sky_minimal_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require Resource SkyLight
// @ulre uses surface_interface
// @ulre end
// === Surface Function: SkyMinimal ===
// Procedural sky — gradient + atmosphere scatter + sun core/glow
// 无 Material Instance，无贴图。sky UBO 由 FS 模板声明。

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    vec3 dir      = normalize(si.worldPos);   // VS 传来的方向向量
    vec3 to_light = normalize(sky.sun_direction.xyz);

    // ---- sky gradient + atmosphere scatter ----
    float h = clamp(dir.z, 0.0, 1.0);

    vec3 base = sky.base_sky_color.rgb;
    vec3 grad = base * exp2(-(1.0 - h) * 0.8);

    float cos_t   = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float region  = smoothstep(cos(sun_rad * 6.0), 1.0, cos_t);

    float horizon  = 1.0 - h;
    vec3  warmTint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3  scatterMix = mix(grad, warmTint, region * 0.5 * sky.sun_intensity);

    float atmosphere = sqrt(max(0.0, 1.0 - h));
    vec3  sky_color  = mix(grad, scatterMix, atmosphere * 0.7);

    // ---- sun disc (hard core + glow) ----
    float sun_cos = cos(sun_rad);
    float coreN   = clamp((cos_t - sun_cos) / max(1e-5, 1.0 - sun_cos), 0.0, 1.0);

    float hard = pow(coreN, 100.0) * pow(h, 1.0 / 1.65);
    float glow = pow(coreN, 6.0)   * pow(h, 0.5);

    float sunMask  = clamp(hard + glow, 0.0, 1.0);
    vec3  sun_color = sky.sun_color.rgb * sunMask * sky.sun_intensity;

    // ---- output ----
    SurfaceOutput so;
    so.baseColor = sky_color + sun_color;
    so.alpha     = 1.0;
    return so;
}
