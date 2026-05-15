#ifndef ULRE_SURFACE_PURECOLOR3D_SURFACE_GLSL
#define ULRE_SURFACE_PURECOLOR3D_SURFACE_GLSL

#include "common/surface_interface.glsl"
#include "common/ssbo_material_instance.glsl"

SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialBindingInstance mi = GetMaterialBindingInstance();

    SurfaceOutput so;
    so.baseColor = mi.Color.rgb;
    so.alpha     = mi.Color.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si)
{
    return GetMaterialBindingInstance().Color.a;
}

#endif // ULRE_SURFACE_PURECOLOR3D_SURFACE_GLSL
