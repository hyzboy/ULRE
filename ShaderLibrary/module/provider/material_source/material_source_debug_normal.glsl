// @ulre begin
// @ulre name material_source_debug_normal
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic WorldNormal
// @ulre require Resource MaterialData
// @ulre uses material_source_interface
// @ulre end

#ifndef DEBUG_NORMAL_SOURCE_GLSL
#define DEBUG_NORMAL_SOURCE_GLSL
#include "module/interface/material_source_interface.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 color = MTL_ROW(sourceInput.dataIndex).color;
    // const vec4 color = MTL_ROW(sourceInput.dataIndex).color;
    const vec3 lightDirection =
        normalize(vec3(0.655386, 0.491539, 0.573462));
    const float intensity =
        0.5 * max(
            dot(sourceInput.surface.worldNormal, lightDirection),
            0.0) + 0.5;
    const vec3 halfVector = normalize(
        lightDirection + sourceInput.surface.viewDir);
    const float specular = max(
        dot(halfVector, sourceInput.surface.worldNormal), 0.0);

    MaterialSourceOutput materialResult;
    materialResult.baseColor =
        intensity * color.rgb
        + specular * pow(specular, 16.0);
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = 1.0;
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
    return 1.0;
}
#endif
