// @ulre begin
// @ulre name ntb_none
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ntb_provider
// @ulre uses ntb_interface
// @ulre uses ntb_orthonormal
// @ulre end
// NTB 槽 — 空对象（Null Object）
//
// “本模板不需要切线空间扰动”是一个实现，而不是一个 #if 分支。
// 返回以世界法线为 N 的任意正交基，N 与 normalize(surface.worldNormal) 完全一致，
// 因此对不消费法线的模板（forward_unlit）与不消费 EvalSurface 的模板
// （shadow caster 只用 EvalAlpha）输出逐字节不变。
//
// 存在理由：material_surface.glsl 的 EvalSurface 无条件调用 GetNTB。若某个模板
// 不提供该实现，`GetNTB` 未声明 —— GLSL 在函数定义处就要解析符号，与调用点是否
// 可达无关，整个翻译单元直接编译失败。所以 NTB 必须是所有使用 material_surface
// 的模板的必需槽，缺省用本模块补位。
#ifndef NTB_NONE_GLSL
#define NTB_NONE_GLSL

#include "common/ntb_interface.glsl"
#include "ntb/ntb_orthonormal.glsl"

NTBSpace GetNTB(NTBInput ntb_input)
{
    return BuildOrthoNTB(normalize(ntb_input.surface.worldNormal));
}

#endif // NTB_NONE_GLSL
