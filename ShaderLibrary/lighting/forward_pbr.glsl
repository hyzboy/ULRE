// @ulre begin
// @ulre name forward_pbr
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Default forward lighting algorithm.
// Direct and indirect providers are selected independently by the compositor.

#ifndef FORWARD_PBR_GLSL
#define FORWARD_PBR_GLSL

#include "common/lighting_interface.glsl"

vec4 EvalLighting(LightingInput lighting)
{
    const vec3 directColor = EvalDirectLighting(lighting);
    vec3 indirectColor = EvalIndirectLighting(lighting);

    const vec3 reflectionF0 =
        mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    indirectColor += lighting.reflectionColor
                   * reflectionF0
                   * (1.0 - lighting.roughness);

    return vec4(
        directColor + indirectColor + lighting.emissive,
        lighting.alpha);
}

#endif // FORWARD_PBR_GLSL
