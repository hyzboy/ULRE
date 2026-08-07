// @ulre begin
// @ulre name forward_flat
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Reference alternate lighting algorithm.
// It proves that an algorithm may ignore all scene-light providers and only
// consume the generated LightingInput contract.

#ifndef FORWARD_FLAT_GLSL
#define FORWARD_FLAT_GLSL

#include "common/lighting_interface.glsl"

vec4 EvalLighting(LightingInput lighting)
{
    return vec4(lighting.baseColor + lighting.emissive, lighting.alpha);
}

#endif // FORWARD_FLAT_GLSL
