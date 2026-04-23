#ifndef ULRE_TONEMAP_SIMPLEREINHARD_GLSL
#define ULRE_TONEMAP_SIMPLEREINHARD_GLSL

#include "util/color_space.glsl"
vec3 ToneMapping(vec3 color)
{
	color *= u_Exposure/(1. + color / u_Exposure);

	return linearTosRGB(color);
}

#endif // ULRE_TONEMAP_SIMPLEREINHARD_GLSL
