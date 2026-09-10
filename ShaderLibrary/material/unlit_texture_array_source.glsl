// @ulre begin
// @ulre name unlit_texture_array_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment required
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end

#ifndef UNLIT_TEXTURE_ARRAY_SOURCE_GLSL
#define UNLIT_TEXTURE_ARRAY_SOURCE_GLSL

#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

vec4 SampleMaterialColor(MaterialSourceInput source_input)
{
    const uvec2 texture_reference =
        MTL_TEX(source_input.dataIndex).tex_base_color;
    return Sample2DArray(
        texture_reference.x,
        TrilinearSampler,
        source_input.surface.uv0,
        float(texture_reference.y));
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput source_input)
{
    const vec4 color = SampleMaterialColor(source_input);
    MaterialSourceOutput material_output;
    material_output.baseColor = color.rgb;
    material_output.metallic = 0.0;
    material_output.roughness = 1.0;
    material_output.fresnel = 0.0;
    material_output.normalScale = 1.0;
    material_output.ao = 1.0;
    material_output.emissive = vec3(0.0);
    material_output.alpha = color.a;
    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    return SampleMaterialColor(source_input).a;
}

#endif
