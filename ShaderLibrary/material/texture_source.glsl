// @ulre begin
// @ulre name texture_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment required
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end

#ifndef TEXTURE_SOURCE_GLSL
#define TEXTURE_SOURCE_GLSL
#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

vec4 SampleMaterialColor(MaterialSourceInput sourceInput)
{
    const uvec2 texture_reference =
        MTL_TEX(sourceInput.dataIndex).tex_base_color;
    return Sample2DArray(
        texture_reference.x,
        TrilinearSampler,
        sourceInput.surface.uv0,
        float(texture_reference.y));
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 color = SampleMaterialColor(sourceInput);
    MaterialSourceOutput materialResult;
    materialResult.baseColor = color.rgb;
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
    return SampleMaterialColor(sourceInput).a;
}
#endif
