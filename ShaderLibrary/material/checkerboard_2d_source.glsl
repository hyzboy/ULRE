// @ulre begin
// @ulre name checkerboard_2d_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre uses material_source_interface
// @ulre end

#ifndef CHECKERBOARD_2D_SOURCE_GLSL
#define CHECKERBOARD_2D_SOURCE_GLSL

#include "common/material_source_interface.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const ivec2 cell = ivec2(floor(sourceInput.surface.screenPos / 32.0));
    const int parity = (cell.x + cell.y) & 1;
    const vec3 checkerColor = mix(
        vec3(0.0),
        vec3(0.35),
        float(parity));

    MaterialSourceOutput materialResult;
    materialResult.baseColor = checkerColor;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = 1.0;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return 1.0;
}

#endif // CHECKERBOARD_2D_SOURCE_GLSL
