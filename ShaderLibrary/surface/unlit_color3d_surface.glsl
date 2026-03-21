
#include "common/surface_interface.glsl"

struct MaterialInstance
{
    vec4 color;     };


#include "common/ssbo_material_instance.glsl"
SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialInstance mi = GetMaterialInstance();

    SurfaceOutput so;
    so.baseColor = mi.color.rgb;
    so.alpha     = mi.color.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si)
{
    return GetMaterialInstance().color.a;
}
