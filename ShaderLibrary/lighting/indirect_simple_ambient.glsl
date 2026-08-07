// @ulre begin
// @ulre name indirect_simple_ambient
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Indirect Lighting — Simple Ambient Color
#ifndef INDIRECT_SIMPLE_AMBIENT_GLSL
#define INDIRECT_SIMPLE_AMBIENT_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalIndirectLighting(
    LightingInput lighting
) {
    return lighting.ambientColor * lighting.baseColor
         * (1.0 - lighting.metallic) * lighting.ao * 0.2;
}

#endif // INDIRECT_SIMPLE_AMBIENT_GLSL
