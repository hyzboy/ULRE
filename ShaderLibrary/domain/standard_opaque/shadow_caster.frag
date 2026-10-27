#version 450
// StandardOpaque 域 · ShadowDepth pass（**无 alpha 覆盖判定**）的主着色器。
//
// 这一档是纯空体：没有 varying、没有 include、没有 sampled 输出。
// 存在意义只是「占位」——depth pass 仍然需要一个可编译的 fragment 阶段，
// 而该 fragment 什么都不做。不要为了"整齐"往这里加任何东西，
// 否则会平白改变 depth pass 的管线状态与 SPIR-V。
//
// 有 alpha 覆盖判定的深度 pass 见同目录 shadow_caster_masked.frag。

#include "domain/root_bindings.glsl"
#include "generated/frag_options.glsl"

void main()
{
}
