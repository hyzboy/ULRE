#ifndef ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL
#define ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL

// ──────────────────────────────────────────────────────────────────────────
// frag_forward_main.glsl -- Unified forward fragment entry point.
//
// Injected by C++ CompositorAssembler after:
//   frag_forward_ubo.glsl  (UBO declarations, surface_interface, varyings)
//   [skylight_*.glsl]      (if NEEDS_SKY / ENABLE_LIGHTING)
//   [lighting_*.glsl]      (if ENABLE_LIGHTING)
//   [surface/*.glsl]       (EvalSurface implementation)
//
// Control defines (set by CompositorAssembler before this chain):
//   HAS_POSITION / HAS_NORMAL / HAS_TANGENT / HAS_TEXCOORD
//   HAS_COLOR / HAS_BILLBOARD_TEXCOORD / HAS_DIRECTION / HAS_LUMINANCE / HAS_CLIP_POS
//   ENABLE_LIGHTING   -- EvalLighting path
//   ALPHA_MODE_MASKED -- hard clip at ALPHA_THRESHOLD (default 0.5)
//   ALPHA_MODE_DITHER -- Bayer 4x4 ordered dither discard
//   TEXTURE_ARRAY_MODE

layout(location=0) out vec4 outColor;

#if defined(ALPHA_MODE_MASKED) || defined(ALPHA_MODE_DITHER)
#include "util/alpha_test.glsl"
#endif

#include "compositor/frag_input_resolve.glsl"
#include "compositor/frag_output_compose.glsl"

void main()
{
#ifdef TEXTURE_ARRAY_MODE
    #ifdef MATERIAL_INSTANCE_ID_OVERRIDE
        _ULRE_InitTextureLayerIndices(MATERIAL_INSTANCE_ID_OVERRIDE);
    #else
        _ULRE_InitTextureLayerIndices();
    #endif
#endif

    SurfaceInput  si = ResolveSurfaceInput();
    SurfaceOutput so = EvalSurface(si);
    ComposeOutput(so, si);
}

#endif // ULRE_COMPOSITOR_FRAG_FORWARD_MAIN_GLSL
