// @ulre begin
// @ulre name text_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require Resource MaterialData
// @ulre ssbo mtl TransmissionSurface 0 Fragment required
// @ulre texture TextureText MaterialSampler BaseColor sampler2D Fragment required
// @ulre uses material_source_interface
// @ulre end

#ifndef TEXT_SOURCE_GLSL
#define TEXT_SOURCE_GLSL
#include "common/material_source_interface.glsl"
layout(set=TEX_SET, binding=TEX_BINDING)
    uniform sampler2D TextureText;

vec4 GetTextColor(MaterialSourceInput sourceInput)
{
    return unpackUnorm4x8(
        MTL_DATA.data[sourceInput.dataIndex].TextColor);
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 textColor = GetTextColor(sourceInput);
    const float luminance =
        texture(TextureText, sourceInput.surface.uv0).r;
    MaterialSourceOutput materialResult;
    materialResult.baseColor = textColor.rgb * luminance;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = textColor.a;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return GetTextColor(sourceInput).a;
}
#endif
