// @ulre begin
// @ulre name luminance_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic Luminance
// @ulre require Resource MaterialData
// @ulre ssbo mtl_private_data EmissiveSurface 0 Fragment required
// @ulre uses material_source_interface
// @ulre end

#ifndef LUMINANCE_SOURCE_GLSL
#define LUMINANCE_SOURCE_GLSL
#include "common/material_source_interface.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
#ifdef ULRE_MATERIAL_ARENA_BDA
    const vec4 color = MTL_ROW(sourceInput.dataIndex)->color;
#else
    const vec4 color = MTL_DATA.data[sourceInput.dataIndex].color;
#endif
    MaterialSourceOutput materialResult;
    materialResult.baseColor =
        sourceInput.surface.luminance * color.rgb;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = color.a;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
#ifdef ULRE_MATERIAL_ARENA_BDA
    return MTL_ROW(sourceInput.dataIndex)->color.a;
#else
    return MTL_DATA.data[sourceInput.dataIndex].color.a;
#endif
}
#endif
