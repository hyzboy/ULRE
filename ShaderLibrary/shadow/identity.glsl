// @ulre begin
// @ulre name identity_shadow
// @ulre kind Utility
// @ulre priority 0
// @ulre slot shadow_provider
// @ulre provides_capability shadow_factor
// @ulre end

#ifndef IDENTITY_SHADOW_GLSL
#define IDENTITY_SHADOW_GLSL

/// 无阴影实现的占位 provider：签名与 pcf_shadow 的 GetShadowFactor 一致
/// （D3 起带 data_index），恒返回全受光。
float GetShadowFactor(SurfaceInput surface, uint data_index)
{
    return 1.0;
}

#endif
