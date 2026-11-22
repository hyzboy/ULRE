// @ulre begin
// @ulre name material_source_debug_attribute
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic UV0
// @ulre require Resource MaterialData
// @ulre texture_reference base_color Fragment optional fallback
// @ulre texture_reference roughness Fragment optional fallback
// @ulre texture_reference metallic Fragment optional fallback
// @ulre texture_reference occlusion Fragment optional fallback
// @ulre texture_reference opacity_mask Fragment optional fallback
// @ulre texture_reference emissive Fragment optional fallback
// @ulre texture_reference orm Fragment optional fallback
// @ulre sampler_reference Trilinear
// @ulre sampler_reference TrilinearAniso
// @ulre sampler_reference Linear
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// 调试视图通道源（MaterialDomain::DebugAttribute 专用）
//
// 通道在**编译期**由 frag_options 的宏选定（配方 compile_defines 发射），
// 因此每个通道各自得到独立的 program / pipeline / 缓存项，没有运行时分支。
//
// 为什么把结果放在 baseColor 上：本领域走 ForwardUnlit 模板，输出策略是
// `output_policy_flat`，`EvalLighting()` 直接返回 `baseColor + emissive`。
// 把通道写进 baseColor 就等于"原样输出"，不需要任何光照计算。

#ifndef DEBUG_ATTRIBUTE_SOURCE_GLSL
#define DEBUG_ATTRIBUTE_SOURCE_GLSL

#include "module/interface/material_source_interface.glsl"
#include "module/descriptor/bindless_textures.glsl"

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    // 行布局与 StandardOpaque 逐字节同构 ⇒ 直接读被诊断材质的 PBR 行。
    PBRSurfaceRow material_data = MTL_ROW(sourceInput.dataIndex);

    const uint material_data_index = sourceInput.dataIndex;
    const vec2 material_uv = sourceInput.surface.uv0;

    // optional 槽未绑定时 SampleOptional 以 1.0 兜底（与 PBR 源同义）。
    const vec3 base_color =
        material_data.base_color.rgb
        * SampleOptional(MTL_TEX(material_data_index).tex_base_color,
                         TrilinearAnisoSampler, material_uv, vec4(1.0)).rgb;

    const vec3 emissive =
        SampleOptional(MTL_TEX(material_data_index).tex_emissive,
                       TrilinearAnisoSampler, material_uv, vec4(0.0)).rgb;

    // ORM 打包图优先（R=AO / G=Roughness / B=Metallic），否则回落到独立槽。
    float roughness = material_data.roughness;
    float metallic = material_data.metallic;
    float ao = 1.0;

    const uvec2 orm_ref = MTL_TEX(material_data_index).tex_orm;
    if (orm_ref.x != 0u)
    {
        const vec3 orm =
            Sample2DArray(orm_ref.x, TrilinearAnisoSampler, material_uv,
                          float(orm_ref.y)).rgb;
        ao = clamp(orm.r, 0.0, 1.0);
        roughness = clamp(roughness * orm.g, 0.04, 1.0);
        metallic = clamp(metallic * orm.b, 0.0, 1.0);
    }
    else
    {
        roughness =
            clamp(roughness
                      * SampleOptional(MTL_TEX(material_data_index).tex_roughness,
                                       TrilinearAnisoSampler, material_uv,
                                       vec4(1.0)).r,
                  0.04, 1.0);
        metallic =
            clamp(metallic
                      * SampleOptional(MTL_TEX(material_data_index).tex_metallic,
                                       TrilinearAnisoSampler, material_uv,
                                       vec4(1.0)).r,
                  0.0, 1.0);
        ao = SampleOptional(MTL_TEX(material_data_index).tex_occlusion,
                            TrilinearAnisoSampler, material_uv, vec4(1.0)).r;
    }

    vec3 channel;

#if defined(HGL_DEBUG_VIEW_BASE_COLOR)
    channel = base_color;
#elif defined(HGL_DEBUG_VIEW_SURFACE)
    channel = vec3(roughness, metallic, ao);
#elif defined(HGL_DEBUG_VIEW_EMISSIVE)
    channel = emissive;
#elif defined(HGL_DEBUG_VIEW_UV0)
    channel = vec3(fract(material_uv), 0.0);
#else
    // 默认（含 HGL_DEBUG_VIEW_WORLD_NORMAL）：世界法线编码到 [0,1]。
    // 8bit 量化下每分量误差 <= 1/255，模长误差上限约 0.014 ⇒ 分析侧判据取 0.05。
    const vec3 normal = sourceInput.surface.worldNormal;
    const float normal_length_squared = dot(normal, normal);
    channel = normal_length_squared > 1e-20 && !isinf(normal_length_squared)
              && !isnan(normal_length_squared)
        ? normal * inversesqrt(normal_length_squared) * 0.5 + 0.5 : vec3(0.5);
#endif

    MaterialSourceOutput materialResult;
    materialResult.baseColor = channel;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = 1.0;
    materialResult.clearCoat = 0.0;
    materialResult.clearCoatRoughness = 0.0;
    materialResult.sheenColor = vec3(0.0);
    materialResult.sheenRoughness = 0.0;
    materialResult.anisotropy = 0.0;
    materialResult.subsurfacePower = 0.0;
    materialResult.thickness = 0.0;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput /*sourceInput*/)
{
    // 调试通道恒不透明：半透明图元会与背景混合，破坏像素统计的可比性。
    return 1.0;
}

#endif // DEBUG_ATTRIBUTE_SOURCE_GLSL
