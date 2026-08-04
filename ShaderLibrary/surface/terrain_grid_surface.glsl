// @ulre begin
// @ulre name terrain_grid_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require Resource Camera
// @ulre uses surface_interface
// @ulre end
// === Surface Function: Terrain Grid ===
// 硬编码太阳方向 + Blinn-Phong 光照，保持与旧版行为一致
// 注意：si.worldPos 实际是 clip-space position（原始设计如此）

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    const vec3 SUN_DIRECTION = normalize(vec3(0.655386, 0.491539, 0.573462));
    const vec3 SUN_COLOR     = vec3(1.0, 1.0, 1.0);
    const vec3 BASE_COLOR    = vec3(0.45, 0.6, 0.35);

    vec3 n = normalize(si.worldNormal);

    // Diffuse (half-Lambert)
    float intensity = 0.5 * max(dot(n, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * BASE_COLOR;

    // Blinn-Phong specular (preserving original clip-space + camera.pos trick)
    vec3 spec_color = vec3(0.0);
    if (intensity > 0.0)
    {
        vec3 half_vector = normalize(SUN_DIRECTION + normalize(si.worldPos + camera.pos));
        float spec = max(dot(half_vector, n), 0.0);
        spec_color = spec * pow(spec, 16.0) * SUN_COLOR;
    }

    SurfaceOutput so;
    so.baseColor = direct_color + spec_color;
    so.alpha     = 1.0;
    so.normal    = n;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint materialInstanceID)
{
    return 1.0;
}
