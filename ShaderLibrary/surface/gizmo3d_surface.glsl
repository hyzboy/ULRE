
#include "common/surface_interface.glsl"

#include "common/ssbo_material_instance.glsl"
SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialInstance mi = GetMaterialInstance();

    const vec3 SUN_DIRECTION = vec3(0.655386, 0.491539, 0.573462);
    const vec3 SUN_COLOR     = vec3(1.0, 1.0, 1.0);

    float intensity = 0.5 * max(dot(si.worldNormal, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * mi.Color.rgb;

    vec3 spec_color = vec3(0.0);
    if (intensity > 0.0)
    {
        vec3 half_vector = normalize(SUN_DIRECTION + si.viewDir);
        float specular = max(dot(half_vector, si.worldNormal), 0.0);
        spec_color = specular * pow(specular, 16.0) * SUN_COLOR;
    }

    SurfaceOutput so;
    so.baseColor = direct_color + spec_color;
    so.alpha     = 1.0;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si)
{
    return 1.0;
}
