#version 450

#define NEEDS_CAMERA
#define NEEDS_SKY
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "compositor/frag_forward_ubo.glsl"
#include SKYLIGHT_FUNCTION_FILE
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
