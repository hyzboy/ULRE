#ifndef ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL
#define ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL

// ──────────────────────────────────────────────────────────────────────────
// frag_forward_main.glsl -- Unified forward fragment entry point.
//
// Injected by C++ CompositorAssembler after:
//   frag_forward_ubo.glsl  (UBO declarations, surface_interface, varyings)
//   [skylight_*.glsl]      (if NEEDS_SKY / ENABLE_LIGHTING)
//   [lighting_*.glsl]      (if ENABLE_LIGHTING)
//   [fragment_provider/*.glsl]  (if PCG_FRAGMENT_PROVIDER is defined)
//   [surface/*.glsl]       (EvalSurface implementation)
//
// Control defines (set by CompositorAssembler before this chain):
//   HAS_POSITION / HAS_NORMAL / HAS_TANGENT / HAS_TEXCOORD
//   HAS_COLOR / HAS_DIRECTION / HAS_LUMINANCE / HAS_CLIP_POS
//   ENABLE_LIGHTING   -- EvalLighting path
//   ALPHA_MODE_MASKED -- hard clip at ALPHA_THRESHOLD (default 0.5)
//   ALPHA_MODE_DITHER -- Bayer 4x4 ordered dither discard
//   TEXTURE_ARRAY_MODE
//   PCG_FRAGMENT_PROVIDER -- fragment provider supplies GetSurfaceInput()
//                            directly; frag_input_resolve.glsl is skipped.

layout(location=0) out vec4 outColor;

#if defined(ALPHA_MODE_MASKED) || defined(ALPHA_MODE_DITHER)
#include "util/alpha_test.glsl"
#endif

#ifndef PCG_FRAGMENT_PROVIDER
#include "compositor/frag_input_resolve.glsl"
#endif

#include "compositor/frag_output_compose.glsl"

void main()
{
#ifdef PCG_FRAGMENT_PROVIDER
    SurfaceInput  si = GetSurfaceInput();
#else
    SurfaceInput  si = ResolveSurfaceInput();
#endif
    SurfaceOutput so = EvalSurface(si);
    ComposeOutput(so, si);
}

#endif // ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL

