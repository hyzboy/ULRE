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
#include "sky/sky_atmosphere.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    vec3 dir      = normalize(si.worldPos);   // VS 传来的方向向量
    vec3 to_light = normalize(sky.sun_direction.xyz);

    float h = clamp(dir.z, 0.0, 1.0);
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    vec3 sky_color = EvalSkyAtmosphere(dir);

    // ---- sun disc (hard core + glow) ----
    float sun_cos = cos(sun_rad);
    float coreN   = clamp((cos_t - sun_cos) / max(1e-5, 1.0 - sun_cos), 0.0, 1.0);

    float hard = pow(coreN, 100.0) * pow(max(h, 1e-3), 1.0 / 1.65);
    float glow = pow(coreN, 6.0)   * pow(max(h, 1e-3), 0.5);

    float sunMask  = clamp(hard + glow, 0.0, 1.0);
    vec3  sun_color = sky.sun_color.rgb * sunMask * sky.sun_intensity;

    // ---- output ----
    SurfaceOutput so;
    so.baseColor = sky_color + sun_color;
    so.alpha     = 1.0;
    return so;
}
