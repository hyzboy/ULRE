// @ulre begin
// @ulre name full_shadow
// @ulre kind Utility
// @ulre priority 0
// @ulre slot shadow_provider
// @ulre provides_capability shadow_factor
// @ulre uses surface_interface
// @ulre end
// 阴影槽 — 全阴影（Null Object 的反面）
//
// 签名与 pcf_shadow / identity_shadow 完全一致，恒返回 0.0（全遮挡）。
// 用途：① “明确不接受直接光照”的调试/风格化路径；② 验证阴影轴
// 确实只是一次 include 替换——换成本模块后所有受光面应彻底变黑，
// 而主着色器、合成器、其它算法槽一个字都不用动。
#ifndef FULL_SHADOW_GLSL
#define FULL_SHADOW_GLSL

#include "common/surface_interface.glsl"

float GetShadowFactor(SurfaceInput surface, uint data_index)
{
    return 0.0;
}

#endif // FULL_SHADOW_GLSL
