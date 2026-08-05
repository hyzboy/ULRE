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
    SurfaceOutput surf,
    NTBSpace ntb,
    vec3 viewDir,
    vec3 skyAmbientColor
) {
    return skyAmbientColor * surf.baseColor * (1.0 - surf.metallic) * surf.ao * 0.2;
}

#endif // INDIRECT_SIMPLE_AMBIENT_GLSL
