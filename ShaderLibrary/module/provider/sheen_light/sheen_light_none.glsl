// @ulre begin
// @ulre name sheen_light_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot sheen_light_provider
// @ulre provides_capability direct_sheen
// @ulre uses lighting_interface
// @ulre end
// 绒面槽 — 空对象（Null Object）
//
// 恒返回 0；能量损失补偿项 (1 - sheenEnergyLoss) 在 LightingModel 中
// 由字面量 0 的 sheenColor / sheenRoughness 常量折叠掉。
#ifndef SHEEN_NONE_GLSL
#define SHEEN_NONE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSheenLight(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // SHEEN_NONE_GLSL
