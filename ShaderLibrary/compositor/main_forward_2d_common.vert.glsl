#version 450

// 2D vertex shader entry — pure color only (no texture, no vertex color).
// Reuses the 3D vert_forward_main.glsl template via VERT_INPUT_2D.

#define VERT_INPUT_2D
#include "compositor/vert_forward_ubo.glsl"
#include "compositor/vert_forward_main.glsl"
