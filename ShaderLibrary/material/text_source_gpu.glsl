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
// Dual-path text rendering:
//   TEXT_SDF_ENABLED defined → SDF distance field sampling + smoothstep AA (Linear sampler)
//   otherwise               → raw bitmap grayscale sampling (Nearest sampler)

#ifndef TEXT_SOURCE_GPU_GLSL
#define TEXT_SOURCE_GPU_GLSL
#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

#ifdef TEXT_SDF_ENABLED
#define TEXT_SAMPLER LinearSampler
#else
#define TEXT_SAMPLER NearestSampler
#endif

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    const vec4 textColor = sourceInput.surface.vertexColor;
    const float rawSample =
        Sample2D(
            mtl_texture_layer_rows.data[sourceInput.dataIndex].base_color,
            TEXT_SAMPLER,
            sourceInput.surface.uv0).r;

    MaterialSourceOutput materialResult;

#ifdef TEXT_SDF_ENABLED
    // SDF 路径：解码距离场 + smoothstep 抗锯齿
    const float sdf = rawSample * 2.0 - 1.0;
    const float smoothing = fwidth(sdf) * 1.0;
    const float alpha = smoothstep(-smoothing, smoothing, sdf);
    materialResult.baseColor = textColor.rgb * alpha;
    materialResult.alpha = textColor.a * alpha;
#else
    // 原始位图路径：直接灰度调制
    materialResult.baseColor = textColor.rgb * rawSample;
    materialResult.alpha = textColor.a;
#endif

    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    const float rawSample =
        Sample2D(
            mtl_texture_layer_rows.data[sourceInput.dataIndex].base_color,
            TEXT_SAMPLER,
            sourceInput.surface.uv0).r;

#ifdef TEXT_SDF_ENABLED
    const float sdf = rawSample * 2.0 - 1.0;
    const float smoothing = fwidth(sdf) * 1.0;
    return sourceInput.surface.vertexColor.a * smoothstep(-smoothing, smoothing, sdf);
#else
    return sourceInput.surface.vertexColor.a * rawSample;
#endif
}
#endif
