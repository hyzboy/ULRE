#ifndef ULRE_SURFACE_FRAGCOORD_SURFACE_GLSL
#define ULRE_SURFACE_FRAGCOORD_SURFACE_GLSL

// surface/fragcoord_surface.glsl
//
// Minimal PCG surface for FullscreenTriangle + PCG_FragCoord.
// Returns normalised screen UV as an RGB gradient — useful as a
// diagnostic / regression baseline for the VS-PCG + FS-PCG path.
//
// si.uv0 / si.screenPos are populated by fragment_provider/pcg_fragcoord.glsl
// as gl_FragCoord.xy / viewport.canvas_resolution.

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si)
{
    SurfaceOutput so;
    so.baseColor = vec3(si.screenPos, 0.5);
    so.alpha     = 1.0;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

#endif // ULRE_SURFACE_FRAGCOORD_SURFACE_GLSL
