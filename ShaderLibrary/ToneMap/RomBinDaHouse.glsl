#ifndef ULRE_TONEMAP_ROMBINDAHOUSE_GLSL
#define ULRE_TONEMAP_ROMBINDAHOUSE_GLSL

#include "util/color_space.glsl"
vec3 ToneMapping(vec3 color)
{
    color = exp( -1.0 / ( 2.72*color + 0.15 ) );

	return linearTosRGB(color);
}

#endif // ULRE_TONEMAP_ROMBINDAHOUSE_GLSL
