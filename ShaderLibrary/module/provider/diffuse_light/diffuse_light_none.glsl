// @ulre begin
// @ulre name diffuse_light_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot diffuse_light_provider
// @ulre provides_capability direct_diffuse
// @ulre uses lighting_interface
// @ulre end
// 漫反射槽 — 空对象（Null Object）
//
// “无漫反射响应”是一种**真实存在的实现**，不是条件编译分支：
// 恒返回 0，GPU 编译器会把它整条内联成常量并 DCE 掉，
// 因此主着色器里永远不需要 #if 去判断“要不要漫反射”。
#ifndef DIFFUSE_NONE_GLSL
#define DIFFUSE_NONE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalDiffuseLight(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // DIFFUSE_NONE_GLSL
