#version 450

#define ALPHA_MODE_MASKED
#define HAS_POSITION
#define HAS_NORMAL
#define HAS_TEXCOORD
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
