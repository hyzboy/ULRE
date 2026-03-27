#version 450

#define NEEDS_CAMERA
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
