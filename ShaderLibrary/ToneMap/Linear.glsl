#ifndef ULRE_TONEMAP_LINEAR_GLSL
#define ULRE_TONEMAP_LINEAR_GLSL

#include "util/color_space.glsl"
vec3 ToneMapping(vec3 color)
{
	color = clamp(u_Exposure * color, 0., 1.);

	return linearTosRGB(color);
}

#endif // ULRE_TONEMAP_LINEAR_GLSL
