#version 450
// Sky 域 · ForwardColor pass 的**物理固定**主着色器（天空盒变体）。
//
// 没有光照：直接把方向向量当"表面位置"送入材质源，输出 baseColor×alpha。
// 没有 OutputPolicy 模块——`WriteMaterialOutput` 是 pass 级的一行定义，
// 不是可替换策略。
//
// `fragDirection` 落在 **location 6**，不是 0：Sky 的 varying 槽位沿用
// `MaterialVertexVaryingConfig` 的位序（DataIndexID=0，Direction=6）。
// 改这个数字必须同步改顶点阶段的输出布局。
//
// 等价性：SPIR-V 尺寸与 reflection 均与旧 ShaderGen 产物全等。
// 无 cube 采样的 SkyMinimal 使用同域独立主着色器 `sky_minimal.frag`。

#include "domain/root_bindings.glsl"
#include "domain/sky_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/provider/sky/sky_atmosphere.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/provider/surface/surface_sky_cube.glsl"
#include "module/interface/alpha_compositor.glsl"
// 阶段间 varying 的输入声明来自 `module/stage/interstage_varying.glsl` 的片段段：
// 置位哪些语义由 `generated/common_options.glsl` 的 `HGL_VARYING_*` 决定，与网格
// 阶段读的是同一个宏集合，两端口径不可能分叉。派生量（viewDir / screenPos /
// 面法线兜底）仍在本文件里按领域赋值。
#define HGL_FRAGMENT_INPUT_VARYINGS
#include "module/stage/interstage_varying.glsl"
layout(location=0) out vec4 outColor;
void WriteMaterialOutput(vec4 value) { outColor = value * fragVertexColor; }

void main()
{
    SurfaceInput si;
    si.worldPos = fragDirection;
    si.worldNormal = normalize(fragDirection);
    si.uv0 = vec2(0.0);
    si.uv1 = vec2(0.0);
    si.vertexColor = fragVertexColor;
    si.viewDir = fragDirection;
    si.screenPos = vec2(0.0);
    si.luminance = 0.0;
    SurfaceOutput so = EvalSurface(si, 0u);
    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));
}
