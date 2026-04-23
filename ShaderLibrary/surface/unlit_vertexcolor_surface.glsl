#ifndef ULRE_SURFACE_UNLIT_VERTEXCOLOR_SURFACE_GLSL
#define ULRE_SURFACE_UNLIT_VERTEXCOLOR_SURFACE_GLSL

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si)
{
    SurfaceOutput so;
    so.baseColor = si.vertexColor.rgb;
    so.alpha     = si.vertexColor.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si)
{
    return si.vertexColor.a;
}

#endif // ULRE_SURFACE_UNLIT_VERTEXCOLOR_SURFACE_GLSL
