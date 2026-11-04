// @ulre begin
// @ulre name subsurface_light_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot subsurface_light_provider
// @ulre provides_capability direct_subsurface
// @ulre uses lighting_interface
// @ulre end
// 次表面槽 — 空对象（Null Object）
//
// 恒返回 0：基线路径上 wrap 漫反射与薄壳背散射整条链被 DCE。
#ifndef SUBSURFACE_NONE_GLSL
#define SUBSURFACE_NONE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSubsurfaceLight(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // SUBSURFACE_NONE_GLSL
