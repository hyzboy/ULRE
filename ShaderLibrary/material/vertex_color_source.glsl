// @ulre begin
// @ulre name vertex_color_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
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
    // The common surface contract applies vertexColor once after evaluating
    // the material source. This provider contributes only the white base.
    materialResult.baseColor = vec3(1.0);
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
#endif
