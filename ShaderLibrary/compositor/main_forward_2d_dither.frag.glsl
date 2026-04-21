#version 450

// LEGACY TEMPLATE: compatibility-only file.
// Runtime shader assembly uses generated routes in CompositorAssembler.
// SURFACE_FUNCTION_FILE is retained for historical reference.

#define ALPHA_MODE_DITHER
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
