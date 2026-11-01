// @ulre begin
// @ulre name material_source_texture
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment required
// @ulre sampler_reference Trilinear
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre uses texture_projection_2d
// @ulre end

#ifndef TEXTURE_SOURCE_GLSL
#define TEXTURE_SOURCE_GLSL
#include "module/interface/material_source_interface.glsl"
#include "module/descriptor/bindless_textures.glsl"
// 投影表示（2D / cubemap / 八面体展开图）的默认实现。若 frag_options 已经按配方
// 里的资源类型选中了另一份 `TextureProjection_*`，三者共用同一个 include guard，
// 这行 include 退化为空操作 —— 采样点的代码因此完全不变，也没有 #if 分支。
#include "module/provider/material_source/texture_projection/texture_projection_2d.glsl"

vec4 SampleMaterialColor(MaterialSourceInput sourceInput)
{
    const uvec2 texture_reference =
        MTL_TEX(sourceInput.dataIndex).tex_base_color;
    return SampleMaterialProjected(texture_reference, sourceInput.surface.uv0);
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
