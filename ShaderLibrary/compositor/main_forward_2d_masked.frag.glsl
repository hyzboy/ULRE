#version 450

// 2D fragment shader entry — alpha masked pass.
// ALPHA_MODE_MASKED triggers discard when alpha < ALPHA_THRESHOLD.
// BayerDither/Masked/A2C code is fully shared with 3D via frag_forward_main.glsl.

#define ALPHA_MODE_MASKED
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
