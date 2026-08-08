// @ulre begin
// @ulre name sky_cubemap
// @ulre kind Shared
// @ulre priority 0
// @ulre uses descriptor_macros
// @ulre ubo SkyInfo Fragment required
// @ulre end
// CubeMap sky-light implementation. The sampler binding is injected by
// MaterialCompiler from the SkyLightCubeMap manifest requirement.
#ifndef SKY_CUBEMAP_GLSL
#define SKY_CUBEMAP_GLSL

#define HGL_SKY_CUBEMAP 1

#include "common/descriptor_macros.glsl"
#include "ubo/sky_info.glsl"

layout(set=SKY_CUBEMAP_SET, binding=SKY_CUBEMAP_BINDING)
    uniform samplerCube SkyCubemap;

vec3 GetSkyMainLightDir()
{
    return normalize(sky.sun_direction.xyz);
}

vec3 GetSkyMainLightColor()
{
    return max(sky.sun_color.rgb * sky.sun_intensity, vec3(0.05));
}

vec3 SampleSkyCubemap(vec3 direction)
{
    return texture(SkyCubemap, normalize(direction)).rgb;
}

vec3 GetSkyReflectionColor(vec3 direction)
{
    return SampleSkyCubemap(direction);
}

vec3 GetSkyAmbientColor()
{
    return SampleSkyCubemap(vec3(0.0, 0.0, 1.0));
}

#endif
