#ifndef ULRE_FRAG_PCG_FRAGCOORD_GLSL
#define ULRE_FRAG_PCG_FRAGCOORD_GLSL

// fragment_provider/pcg_fragcoord.glsl
// @sfm:require  UBO viewport
//
// MANIFEST: {
//   "contract":       "GetSurfaceInput",
//   "needs_camera":   false,
//   "needs_viewport": true,
//   "samplers":       [],
//   "ubo":            ["ubo_viewport"]
// }
//
// Fragment source: procedural — derives SurfaceInput entirely from gl_FragCoord.
// No varyings from the vertex stage are consumed.
//
// GetSurfaceInput() must be declared when replaces_input_resolve == true.
// CompositorAssembler will include this file instead of frag_input_resolve.glsl
// and will NOT emit the standard ResolveSurfaceInput() call.
//
// Exposed helper:
//   SurfaceInput GetSurfaceInput()
//     Returns a SurfaceInput whose baseColor-relevant fields are derived from
//     gl_FragCoord, suitable for a simple FragCoord→colour debug surface.
//
// Fields populated:
//   si.screenPos   — normalised [0..1] screen UV derived from gl_FragCoord.xy
//                    and viewport.canvas_resolution.
//   si.worldPos    — (0,0,0)  — not meaningful for a fullscreen PCG pass.
//   si.worldNormal — (0,0,1)  — forward-facing default.
//   si.uv0         — same as si.screenPos, for surface files that sample via uv0.
//   all other fields — zeroed / default.

#include "common/surface_interface.glsl"

SurfaceInput GetSurfaceInput()
{
    SurfaceInput si;

    vec2 screen_uv = gl_FragCoord.xy / viewport.canvas_resolution;

    si.worldPos     = vec3(0.0);
    si.worldNormal  = vec3(0.0, 0.0, 1.0);
    si.worldTangent = vec4(1.0, 0.0, 0.0, 1.0);
    si.uv0          = screen_uv;
    si.uv1          = screen_uv;
    si.vertexColor  = vec4(1.0);
    si.viewDir      = vec3(0.0, 0.0, 1.0);
    si.screenPos    = screen_uv;
    si.luminance    = 1.0;

    return si;
}

#endif // ULRE_FRAG_PCG_FRAGCOORD_GLSL
