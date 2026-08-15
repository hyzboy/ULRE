// @ulre begin
// @ulre name texture_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre texture_layer base_color Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end

#ifndef TEXTURE_SOURCE_GLSL
#define TEXTURE_SOURCE_GLSL
#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

vec4 SampleMaterialColor(MaterialSourceInput sourceInput)
{
    const uint handle = mtl_texture_layer_rows.data[sourceInput.dataIndex].base_color;
    return SampleBindless2D(handle, sourceInput.surface.uv0);
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
