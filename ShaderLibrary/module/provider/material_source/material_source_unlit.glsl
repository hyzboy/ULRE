// @ulre begin
// @ulre name material_source_unlit
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre uses material_source_interface
// @ulre end
// Unlit material source provider — reads emissive color from SSBO.
// Used by unlit_color3d_surface and unlit_luminance_surface.

#ifndef UNLIT_SOURCE_GLSL
#define UNLIT_SOURCE_GLSL

#include "module/interface/material_source_interface.glsl"

EmissiveSurfaceData EvalUnlitSource(uint dataIndex)
{
    EmissiveSurfaceRow row = MTL_ROW(dataIndex);
    EmissiveSurfaceData data;
    data.color = row.color;
    return data;
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
    // 高级材质参数：本 provider 无高级数据 ⇒ 显式清零（下游常量折叠）。
    materialResult.clearCoat = 0.0;
    materialResult.clearCoatRoughness = 0.0;
    materialResult.sheenColor = vec3(0.0);
    materialResult.sheenRoughness = 0.0;
    materialResult.anisotropy = 0.0;
    materialResult.subsurfacePower = 0.0;
    materialResult.thickness = 0.0;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return EvalUnlitSource(sourceInput.dataIndex).color.a;
}

#endif // UNLIT_SOURCE_GLSL
