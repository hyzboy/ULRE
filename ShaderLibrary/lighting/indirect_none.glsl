// @ulre begin
// @ulre name indirect_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse|ambient_specular
// @ulre uses lighting_interface
// @ulre end
// 间接光槽 — 空对象（Null Object）
//
// “无间接光 / 无 GI”= 恒返回 vec3(0.0)，而不是 #if 掉调用点。
// 这是对间接光这条轴的**默认关闭**实现：切到本模块只改一行 include，
// 主着色器里 lighting 累加式一行不动。
#ifndef INDIRECT_NONE_GLSL
#define INDIRECT_NONE_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalIndirectLighting(LightingInput lighting)
{
    return vec3(0.0);
}

#endif // INDIRECT_NONE_GLSL
