#version 450

#define ALPHA_MODE_DITHER
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
