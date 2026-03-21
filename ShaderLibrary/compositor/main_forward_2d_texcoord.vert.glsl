#version 450

// 2D vertex shader entry — with UV0 (for textured materials).
// Reuses the 3D vert_forward_main.glsl template via VERT_INPUT_2D + HAS_UV0.

#define VERT_INPUT_2D
#define HAS_UV0
#include "compositor/vert_forward_ubo.glsl"
#include "compositor/vert_forward_main.glsl"
