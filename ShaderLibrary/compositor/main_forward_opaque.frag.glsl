#version 450

#define ENABLE_LIGHTING
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_WORLD_TANGENT
#define HAS_UV0
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
