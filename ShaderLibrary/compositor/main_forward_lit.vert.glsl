#version 450

#define HAS_UV0
#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#ifdef TANGENT_LOCATION
	#define HAS_WORLD_TANGENT
#endif
#include "compositor/vert_forward_ubo.glsl"
#include "compositor/vert_forward_main.glsl"
