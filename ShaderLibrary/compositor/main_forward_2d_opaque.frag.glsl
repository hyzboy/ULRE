#version 450

// 2D fragment shader entry — opaque pass.
// No ENABLE_LIGHTING → unlit path. No ALPHA_MODE_* → transparent passthrough.
// Reuses the 3D frag_forward_main.glsl template.

#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
