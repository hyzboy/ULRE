/// @file LightingContract.h
/// 光照 / 阴影 / 材质标志位与文本 SDF 参数的 C++ 真源。
///
/// 这批数字的漂移后果同样静默：
///   * `shadow_flags` 位错 = 该图元恒受光/恒全黑，无报错；
///   * `LightType` / `LightShadowKind` 错 = 光行被 CPU 与 GPU 用不同语义解读；
///   * `TEXT_SDF_SPREAD` 错 = 距离场码率与勾边宽度同时偏移（粗细系统性错误）。

#pragma once

#include <cstdint>

#include <hgl/shader/contract/ShaderContractMacros.h>

namespace hgl::shader::contract
{
    /// 光源类型（`LightRow.type`）。
    enum class HGL_SHADER_ENUM LightTypeId : uint32_t
    {
        Direction = 0,   ///< 方向光（当前唯一实现；主方向光 = 行 0）
    };

    /// 光源行 `flags` 位（GLSL 宏 `LIGHT_FLAG_*`）。
    enum class HGL_SHADER_ENUM LightFlag : uint32_t
    {
        Enabled    = 1u << 0,   ///< 参与光照
        CastShadow = 1u << 1,   ///< 该光投阴影（机制见 `shadow_kind`）
    };

    /// 阴影机制种类（`LightRow.shadow_kind`，GLSL 宏 `LIGHT_SHADOW_*`）。
    enum class HGL_SHADER_ENUM LightShadowKindId : uint32_t
    {
        None = 0,   ///< 不投阴影
        CSM  = 1,   ///< 级联阴影（主方向光当前唯一机制）
    };

    /// 材质实例行 `shadow_flags` 位（GLSL 宏 `HGL_MATERIAL_SHADOW_FLAG_*`）。
    enum class HGL_SHADER_ENUM MaterialShadowFlag : uint32_t
    {
        NoReceive = 1u << 0,   ///< 置位 = 该图元**不接收**阴影（不置位 = 正常接收）
    };

    /// 阴影 PCF 采样参数默认值。
    enum class HGL_SHADER_ENUM ShadowPcfPoissonTaps : uint32_t
    {
        Count = 16,   ///< Poisson 盘采样数（GLSL 宏 `HGL_SHADOW_PCF_POISSON_TAPS`）
    };

    /// 阴影采样法线偏移默认值（GLSL 宏 `HGL_SHADOW_NORMAL_OFFSET`）。
    enum class HGL_SHADER_ENUM ShadowNormalOffset : uint32_t
    {
        Pixels = 1,
    };

    /// 文本距离场传播半径（像素，GLSL 宏 `TEXT_SDF_SPREAD` 为 float）。
    ///
    /// C++ 侧 `hgl::graph::TEXT_SDF_SPREAD`（`FontSource.h`）也是这个值：SDF 位图四周要
    /// 额外扩这么多像素，勾边宽度上限亦由它钳制。GLSL 侧以 `8.0` 形式参与除法。
    enum class HGL_SHADER_ENUM TextSdfSpread : uint32_t
    {
        Pixels = 8,
    };
}//namespace hgl::shader::contract
