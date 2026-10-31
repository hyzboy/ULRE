#version 450
// VertexColor / VertexPalette 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 表面色 100% 来自顶点色（`module/provider/material_source/material_source_vertex_color.glsl`），因此：
//   * `fragVertexColor` 落在 **location 5**（`MaterialVertexVaryingConfig` 的
//     Color 槽位）；
//   * **没有** `fragDataIndexID`，材质数据索引物理固定为常量 0u；
//   * 契约里贴图槽表为空，所以没有贴图表也无可采样器。
//
// VertexPalette 与 VertexColor 共用本文件：两者的顶点语义要求只差
// TransformID/Size（变换与尺寸在顶点阶段已消解），片元阶段完全一致。
//
// 面法线兜底块（`#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK`）**必须保留**：
// 本领域不做法线光照，但生成器对任何前向 pass 都无条件发射它，删掉会让
// SPIR-V 少 92 字节。别「推理掉」看着是死代码的东西。

#include "domain/root_bindings.glsl"
#include "domain/vertex_color_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/provider/output_policy/output_policy_flat.glsl"
#include "module/provider/material_source/material_source_vertex_color.glsl"
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
    // 顶点流不提供法线时的面法线兜底。本领域不做法线光照，但仍可能被
    // 复用型 surface 模块读到 worldNormal，所以保留由 options 选中的这条分支。
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
    const uint materialDataIndex = 0u;
    si.styleID = 0u;
    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);
#if defined(HGL_ALPHA_TEST) || defined(HGL_ALPHA_DITHER)
    // 覆盖（coverage）材质：按材质源的 alpha 槽丢弃片元。
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
