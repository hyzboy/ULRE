#ifndef ULRE_TONEMAP_WHITEPRESERVINGLUMABASEDREINHARD_GLSL
#define ULRE_TONEMAP_WHITEPRESERVINGLUMABASEDREINHARD_GLSL

#include "util/color_space.glsl"
vec3 ToneMapping(vec3 color)
{
	float white = 2.;
	float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
	float toneMappedLuma = luma * (1. + luma / (white*white)) / (1. + luma);
	color *= toneMappedLuma / luma;

	return linearTosRGB(color);
}

#endif // ULRE_TONEMAP_WHITEPRESERVINGLUMABASEDREINHARD_GLSL
