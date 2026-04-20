#version 450

#define HAS_NORMAL
#define HAS_CLIP_POS
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
