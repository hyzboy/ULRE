// @ulre begin
// @ulre name clearcoat_light_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot clearcoat_light_provider
// @ulre provides_capability direct_clearcoat
// @ulre uses lighting_interface
// @ulre end
// 清漆槽 — 空对象（Null Object）
//
// "这材质没有清漆层"是一个实现，而不是一个 #if 分支。恒返回 0，
// 编译器内联成常量后整条清漆链被 DCE 掉，基线性能与无此槽时一致。
#ifndef CLEARCOAT_NONE_GLSL
#define CLEARCOAT_NONE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalClearCoatLight(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // CLEARCOAT_NONE_GLSL
