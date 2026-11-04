// @ulre begin
// @ulre name material_source_luminance
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic Luminance
// @ulre require Resource MaterialData
// @ulre uses material_source_interface
// @ulre end

#ifndef LUMINANCE_SOURCE_GLSL
#define LUMINANCE_SOURCE_GLSL
#include "module/interface/material_source_interface.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 color = MTL_ROW(sourceInput.dataIndex).color;
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
    return MTL_ROW(sourceInput.dataIndex).color.a;
}
#endif
