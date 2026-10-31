#version 450
// Sky 域 · ForwardColor pass 的物理固定主着色器（无贴图大气表面）。
//
// SkyCube 的 cubemap 表面使用独立的 `sky.frag`。SkyMinimal 不需要材质贴图行，
// 必须直接接入 `sky_minimal_surface.glsl`，不能在无 texture_reference 的配方中
// 调用 `MTL_TEX`。

#include "domain/root_bindings.glsl"
#include "domain/sky_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/provider/sky/sky_atmosphere.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/provider/surface/surface_sky_minimal.glsl"
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
    si.styleID = 0u;
    const SurfaceOutput so = EvalSurface(si, 0u);
    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));
}
