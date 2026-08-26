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

// ── 字符样式表（与 mesh 阶段 MeshShaderAssembler 生成的 CharStyleData 严格镜像，
//    std430 40B；CPU/GPU 共用 CharStyle 定义，见 TextCharSSBO.h）──
struct CharStyleData {
    uint  text_color;
    uint  outline_color;
    uint  shadow_color;
    uint  flags;             // bit0 = shadow_enabled
    float italic;
    float bold_px;
    float outline_px;
    uint  shadow_uv_offset;  // packHalf2x16 打包的 UV 偏移
    float scale;             // 缩放因子
    int   rotation;          // 旋转角度 (0/90/180/270)
};
layout(set=PER_OBJECT_SET, binding=TEXT_CHARSTYLE_BINDING, std430) readonly buffer CharStyleDataBuf {
    CharStyleData styles[];
} sbo_char_style;

// SDF 距离场编码跨度（±spread/2 → sdf 域 [-1,1]）：每像素 sdf 增量 = 2.0 / spread
#define TEXT_SDF_SPREAD 8.0

// 文本样式特效合成（EvalMaterialSource / EvalMaterialAlpha 共用）：
//   输入：当前像素已解码的有符号距离场 sdf（[-1,1]，字身边界 = 0）
//   输出：预乘 rgb（baseColor = rgb * coverage，与现有输出约定一致）
//         与最终 alpha（含 textColor.a）
// 零参数（bold=0、outline=0、flags=0）时退化为原始
// smoothstep(-sm, sm, sdf) 公式，视觉完全等价。
void EvalTextStyleEffects(
    const MaterialSourceInput sourceInput,
    const float sdf,
    out vec3 out_rgb,
    out float out_alpha)
{
    const CharStyleData st = sbo_char_style.styles[sourceInput.surface.styleID];
    const vec4 textColor = sourceInput.surface.vertexColor;
    const float du = 2.0 / TEXT_SDF_SPREAD;  // 每像素对应的 sdf 单位
    const float sm = fwidth(sdf);            // 抗锯齿带宽

    // 缩放后的特效参数
    const float scaled_bold = st.bold_px;
    const float scaled_outline = st.outline_px;

    // 加粗：字身边界外扩 bold_px 像素
    const float body_a = smoothstep(-sm, sm, sdf + scaled_bold * du);

    // 勾边：边界外扩 outline_px 像素（outline_px=0 时强制归零，
    // 否则 over 合成会重复叠加字身覆盖率 → 零参数输出不再等价）
    float outline_a = smoothstep(-sm, sm, sdf + scaled_outline * du);
    if (scaled_outline <= 0.0)
        outline_a = 0.0;

    // 阴影（flags bit0）：偏移采样同一距离场；阴影显示为右下偏移
    // → 采样位置向右上（减偏移；纹理 V 轴向下）
    float shadow_a = 0.0;
    if ((st.flags & 1u) != 0u)
    {
        const vec2 off = unpackHalf2x16(st.shadow_uv_offset);
        const float shadow_sdf =
            Sample2D(
                mtl_texture_layer_rows.data[sourceInput.dataIndex].base_color,
                TEXT_SAMPLER,
                sourceInput.surface.uv0 - off).r * 2.0 - 1.0;
        // 阴影跟随加粗（同为字身边界外扩）
        // 扩大 smoothstep 范围（-2px 到 +2px）产生更宽的半透明过渡带
        const float shadow_sm = 2.0 * du;  // 2 像素过渡带宽
        const float shadow_base_a = smoothstep(-shadow_sm, shadow_sm, shadow_sdf + scaled_bold * du);
        // 渐变淡出：利用原始 SDF 距离值调制阴影强度
        // 距离字形越远（sdf 越负），阴影越淡；字身边缘及内部阴影最强
        const float fade = smoothstep(-3.0 * du, 1.0 * du, sdf);
        shadow_a = shadow_base_a * fade;
    }

    // 预乘 over 合成：top = body over outline；final = shadow over top（阴影在最底层）
    const vec3 top_rgb = textColor.rgb * body_a
        + unpackUnorm4x8(st.outline_color).rgb * outline_a * (1.0 - body_a);
    const float top_a = body_a + outline_a * (1.0 - body_a);

    out_rgb = top_rgb
        + unpackUnorm4x8(st.shadow_color).rgb * shadow_a * (1.0 - top_a);
    out_alpha = textColor.a * (top_a + shadow_a * (1.0 - top_a));
}
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
    // SDF 路径：解码距离场 + 样式特效合成（加粗/勾边/阴影）
    const float sdf = rawSample * 2.0 - 1.0;
    vec3 styled_rgb;
    float styled_alpha;
    EvalTextStyleEffects(sourceInput, sdf, styled_rgb, styled_alpha);
    materialResult.baseColor = styled_rgb;
    materialResult.alpha = styled_alpha;
#else
    // 原始位图路径：直接灰度调制（预乘 alpha 输出，与 SDF 路径一致）
    const float coverage = rawSample;
    materialResult.baseColor = textColor.rgb * textColor.a * coverage;
    materialResult.alpha = textColor.a * coverage;
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
    vec3 styled_rgb;
    float styled_alpha;
    EvalTextStyleEffects(sourceInput, sdf, styled_rgb, styled_alpha);
    return styled_alpha;
#else
    return sourceInput.surface.vertexColor.a * rawSample;
#endif
}
#endif
