#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_OPAQUE_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_OPAQUE_VERT_GLSL

#version 450

#define HAS_TEXCOORD
#define HAS_POSITION
#define HAS_NORMAL
#define HAS_TANGENT
#include "compositor/vert_forward_ubo.glsl"
#include "compositor/vert_forward_main.glsl"

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_OPAQUE_VERT_GLSL
