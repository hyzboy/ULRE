#version 450

#define ENABLE_LIGHTING
#define HAS_POSITION
#define HAS_NORMAL
#define HAS_TANGENT
#define HAS_TEXCOORD
#include "compositor/frag_forward_ubo.glsl"
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
