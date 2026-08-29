// @ulre begin
// @ulre name unlit_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre ssbo MaterialPrivateData EmissiveSurface 0 Fragment optional fallback
// @ulre uses material_source_interface
// @ulre end
// Unlit material source provider — reads emissive color from SSBO.
// Used by unlit_color3d_surface and unlit_luminance_surface.

#ifndef UNLIT_SOURCE_GLSL
#define UNLIT_SOURCE_GLSL

#include "common/material_source_interface.glsl"

EmissiveSurfaceData EvalUnlitSource(uint dataIndex)
{
    return MTL_DATA.data[dataIndex];
}

MaterialSourceOutput EvalMaterialSource(
    MaterialSourceInput sourceInput)
{
    const EmissiveSurfaceData data =
        EvalUnlitSource(sourceInput.dataIndex);
    MaterialSourceOutput materialResult;
    materialResult.baseColor = data.color.rgb;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = data.color.a;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return EvalUnlitSource(sourceInput.dataIndex).color.a;
}

#endif // UNLIT_SOURCE_GLSL
