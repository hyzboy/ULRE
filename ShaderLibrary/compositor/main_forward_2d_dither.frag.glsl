#version 450

// 2D fragment shader entry — dither alpha pass.
// ALPHA_MODE_DITHER triggers Bayer 4x4 ordered dither discard.
// BayerDither/Masked/A2C code is fully shared with 3D via frag_forward_main.glsl.

#define ALPHA_MODE_DITHER
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
