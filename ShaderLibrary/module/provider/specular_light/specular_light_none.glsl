// @ulre begin
// @ulre name specular_light_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot specular_light_provider
// @ulre provides_capability direct_specular
// @ulre uses lighting_interface
// @ulre end
// 高光槽 — 空对象（Null Object）
//
// “这材质没有高光”是一个实现，而不是一个 #if 分支。恒返回 0，
// 编译器内联成常量后 DCE 掉，性能与手写无高光着色器一致。
#ifndef SPECULAR_NONE_GLSL
#define SPECULAR_NONE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSpecularLight(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // SPECULAR_NONE_GLSL
