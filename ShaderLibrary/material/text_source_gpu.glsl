// @ulre begin
// @ulre name text_source_gpu
// @ulre kind Utility
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require ProducedSemantic Color
// @ulre texture_layer base_color Fragment required
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// GPU text path: per-vertex color from CharQuad mesh shader varying,
// instead of per-draw color from MaterialData SSBO.

#ifndef TEXT_SOURCE_GPU_GLSL
#define TEXT_SOURCE_GPU_GLSL
#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 textColor = sourceInput.surface.vertexColor;
    const float luminance =
        Sample2D(
            mtl_texture_layer_rows.data[sourceInput.dataIndex].base_color,
            NearestSampler,
            sourceInput.surface.uv0).r;
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
    return sourceInput.surface.vertexColor.a;
}
#endif
