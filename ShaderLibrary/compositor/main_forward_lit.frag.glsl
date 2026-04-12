#version 450

#define NEEDS_CAMERA
#define NEEDS_SKY
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_WORLD_TANGENT
#define HAS_UV0
#if !defined(ULRE_NT_SOURCE_WITH_TANGENT_ATTR) && !defined(ULRE_NT_SOURCE_COMPRESSED)
#define ULRE_NT_SOURCE_WITH_TANGENT_ATTR
#endif
#include "compositor/frag_forward_ubo.glsl"
#include SKYLIGHT_FUNCTION_FILE
#include SURFACE_FUNCTION_FILE
#include "compositor/frag_forward_main.glsl"
