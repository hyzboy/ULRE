#version 450

// 2D vertex shader entry — with vertex color.
// Reuses the 3D vert_forward_main.glsl template via VERT_INPUT_2D + HAS_VERTEX_COLOR.

#define VERT_INPUT_2D
#define HAS_VERTEX_COLOR
#include "compositor/vert_forward_ubo.glsl"
#include "compositor/vert_forward_main.glsl"
