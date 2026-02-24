#pragma once

/// PermutationToRecipe.h — ShaderPermutationKey 枚举值 → ShaderLibrary 模块名映射
///
/// 这是一个纯头文件常量表，无运行时开销。
/// 用于把 FixedMaterialDef 体系的排列键转换为 ShaderTemplateEngine 使用的模块名称字符串。
///
/// 当 ShaderLibrary/modules/ 中新增模块时，只需在此更新对应的映射表即可。

#include<hgl/graph/mtl/FixedMaterialDef.h>

namespace hgl::graph::mtl{

// ─────────────────────────────────────────────────────────────────────────────
// 直接光照模型 → ShaderLibrary/modules/lighting/ 中的模块文件名（不含 .glsl）
// nullptr = 该枚举值尚无对应模块（或不需要光照模块，如 Unlit）
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char *LIGHT_MODEL_MODULE_NAMES[] = {
    nullptr,          // Unlit       — 无模块，跳过光照计算
    "lambert",        // Lambert     — modules/lighting/lambert.glsl
    "blinn_phong",    // BlinnPhong  — modules/lighting/blinn_phong.glsl
    // TODO(task 3.3): 用 pbr_lite.glsl 替换此处的 half_lambert
    // 当前暂用 half_lambert 占位，二者视觉不同，完成 pbr_lite.glsl 后必须更新
    "half_lambert",   // PBR_Lite    — 临时映射，待 pbr_lite.glsl 完成后替换
    "pbr_standard",   // PBR_Full    — modules/lighting/pbr_standard.glsl
    nullptr,          // CelShading  — 待 cel_shading.glsl 完成（任务 3.1）后填入
};

static_assert(
    sizeof(LIGHT_MODEL_MODULE_NAMES)/sizeof(LIGHT_MODEL_MODULE_NAMES[0])
    == size_t(LightModel::RANGE_SIZE),
    "LIGHT_MODEL_MODULE_NAMES size mismatch — update when LightModel enum changes");

// ─────────────────────────────────────────────────────────────────────────────
// 环境光模型 → ShaderLibrary/modules/ambient/ 中的模块文件名（不含 .glsl）
// nullptr = 该枚举值尚无对应模块（或不需要环境光模块，如 FlatColor）
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char *AMBIENT_MODEL_MODULE_NAMES[] = {
    nullptr,          // FlatColor   — 无模块，直接用 MI 中的 packed_tint
    nullptr,          // Hemisphere  — 待 hemisphere.glsl 完成（任务 3.2）后填入
    "ibl_simple",     // IBL         — modules/ambient/ibl_simple.glsl
    // TODO(task post-3.2): 用专用 SH 模块替换此处的通用 ibl
    // 当前暂用 ibl.glsl 占位，完成球谐模块后必须更新
    "ibl",            // IBL_SH      — 临时映射，待球谐模块完成后替换
    nullptr,          // MixedGI     — 待 mixed_gi.glsl 完成后填入
};

static_assert(
    sizeof(AMBIENT_MODEL_MODULE_NAMES)/sizeof(AMBIENT_MODEL_MODULE_NAMES[0])
    == size_t(AmbientModel::RANGE_SIZE),
    "AMBIENT_MODEL_MODULE_NAMES size mismatch — update when AmbientModel enum changes");

// ─────────────────────────────────────────────────────────────────────────────
// 高光通道轴 → 不需要独立 GLSL 模块，由 #define SPECULAR_SPLIT 在模板中控制
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// 阴影接收轴 → ShaderLibrary/modules/shadow/ 中的模块文件名（待添加）
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char *SHADOW_RECEIVE_MODULE_NAMES[] = {
    nullptr,          // None    — 无阴影
    nullptr,          // PCF     — 待 pcf_shadow.glsl 完成后填入
    nullptr,          // PCSS    — 待 pcss_shadow.glsl 完成后填入
};

static_assert(
    sizeof(SHADOW_RECEIVE_MODULE_NAMES)/sizeof(SHADOW_RECEIVE_MODULE_NAMES[0])
    == size_t(ShadowReceive::RANGE_SIZE),
    "SHADOW_RECEIVE_MODULE_NAMES size mismatch — update when ShadowReceive enum changes");

// ─────────────────────────────────────────────────────────────────────────────
// ShaderPermutationKey → quality level 字符串（用于 LoadRecipe）
// 标准映射：按 light+ambient 组合判断质量档位
// ─────────────────────────────────────────────────────────────────────────────
inline const char *PermutationKeyToQualityLevel(const ShaderPermutationKey &key)
{
    if (key.light <= LightModel::Lambert &&
        key.ambient <= AmbientModel::FlatColor)
        return "mobile_low";

    if (key.light <= LightModel::BlinnPhong &&
        key.ambient <= AmbientModel::FlatColor)
        return "mobile_high";

    if (key.light <= LightModel::BlinnPhong)
        return "pc_medium";

    if (key.shadow < ShadowReceive::PCSS)
        return "pc_high";

    return "pc_ultra";
}

}//namespace hgl::graph::mtl
