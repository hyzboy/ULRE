#version 450
// UnlitSolid 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 无光照纯色：表面值直接走 `module/provider/output_policy/output_policy_flat.glsl`。表面来源固定是
// `module/provider/material_source/material_source_unlit.glsl`——它读**材质私有数据行**的 `vec4 color`，
// 本领域契约的 `material_private_data` 因此是 `GlobalSSBOType::EmissiveSurface`
// （行跨度 16），不是 StandardOpaque 的 PBRSurface。绑定布局见
// `domain/emissive_row_bindings.glsl`（该文件由四个 Emissive 行领域共享）。
//
// `fragDataIndexID` 落在 **location 0** 且带 flat perprimitiveEXT：颜色就在
// 材质私有行里，没有它读不到本实例的行。
//
// 必须保留 `#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK` 块（即使本领域不做任何
// 法线光照）：生成器对**任何前向 pass** 都无条件发射这块，它决定
// `si.worldNormal` 是否被填值。删掉它 SPIR-V 会少若干字节——等价性比对里
// 就是实打实的失败。别「推理掉」看着是死代码的东西。

#include "domain/root_bindings.glsl"
#include "domain/emissive_row_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/provider/output_policy/output_policy_flat.glsl"
#include "module/provider/material_source/material_source_unlit.glsl"
#include "module/provider/ntb/ntb_none.glsl"
#include "module/provider/surface/surface_material.glsl"
#include "module/interface/alpha_compositor.glsl"
// 阶段间 varying 的输入声明来自 `module/stage/interstage_varying.glsl` 的片段段：
// 置位哪些语义由 `generated/common_options.glsl` 的 `HGL_VARYING_*` 决定，与网格
// 阶段读的是同一个宏集合，两端口径不可能分叉。派生量（viewDir / screenPos /
// 面法线兜底）仍在本文件里按领域赋值。
#define HGL_FRAGMENT_INPUT_VARYINGS
#include "module/stage/interstage_varying.glsl"
layout(location=0) out vec4 outColor;
void WriteMaterialOutput(vec4 value) { outColor = value; }

void main()
{
    SurfaceInput si;
    si.worldPos = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0 = vec2(0.0);
    si.uv1 = vec2(0.0);
    si.vertexColor = fragVertexColor;
    si.viewDir = vec3(0.0, 0.0, 1.0);
    si.screenPos = gl_FragCoord.xy;
    si.luminance = 1.0;
    si.styleID = 0u;
#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK
    // 顶点流不提供法线时的面法线兜底（本领域不做法线光照，但复用型模块
    // 仍可能读到 worldNormal，所以保留由 options 选中的这条分支）。
    {
        vec3 face_normal_vector =
            cross(dFdx(si.worldPos), dFdy(si.worldPos));
        const float face_normal_length_squared =
            dot(face_normal_vector, face_normal_vector);
        if (face_normal_length_squared > 1e-12)
        {
            face_normal_vector *= inversesqrt(face_normal_length_squared);
            si.worldNormal = gl_FrontFacing
                ? face_normal_vector : -face_normal_vector;
        }
    }
#endif
    const uint materialDataIndex = fragDataIndexID;
    si.styleID = 0u;
    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);
#if defined(HGL_ALPHA_TEST) || defined(HGL_ALPHA_DITHER)
    // ⚠ 守卫必须同时覆盖 HGL_ALPHA_DITHER，否则 dither 材质的覆盖判定会被整个
    // 编译掉（dither 由 HGLApplyAlpha 内部的 HGL_ALPHA_DITHER 分支实现）。
    HGLApplyAlpha(EvalAlpha(si, materialDataIndex));
#endif
    const vec4 finalColor =
        EvalPassEnvironment(PassEnvironmentInput(si, surface, materialDataIndex));
#if defined(HGL_ALPHA_OUTPUT)
    // `alpha_blend` 的 SRC_ALPHA 取**输出 alpha**，不能写光照 alpha（恒为 1）。
    WriteMaterialOutput(vec4(finalColor.rgb, EvalAlpha(si, materialDataIndex)));
#else
    WriteMaterialOutput(HGLComposeColor(finalColor));
#endif
}
