// @ulre begin
// @ulre name vertex_color_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require ProducedSemantic Color
// @ulre uses material_source_interface
// @ulre end

#ifndef VERTEX_COLOR_SOURCE_GLSL
#define VERTEX_COLOR_SOURCE_GLSL
#include "common/material_source_interface.glsl"

MaterialSourceOutput EvalMaterialSource(
    MaterialSourceInput sourceInput)
{
    MaterialSourceOutput materialResult;
    materialResult.baseColor = sourceInput.surface.vertexColor.rgb;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = sourceInput.surface.vertexColor.a;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return sourceInput.surface.vertexColor.a;
}
#endif
