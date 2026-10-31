#version 450
// Checker2D 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 2D 棋盘格（屏幕/UV 空间）：表面来源固定是 `module/provider/material_source/material_source_checkerboard_2d.glsl`，
// 参数全部来自该模块内部的常量，**不读材质私有数据行、不读任何贴图**
// ⇒ 没有 `fragDataIndexID` 输入，`materialDataIndex` 直接是字面 `0u`
// （`EvalSurface` 的入参在无数据行领域里必须喂一个合法下标）。
//
// 本领域是 2D 域：既没有顶点法线也没有世界位置，生成器**不发射**
// `HGL_MATERIAL_FACE_NORMAL_FALLBACK`。`#ifdef` 块保留在源码里是为了让同一份
// 文本结构在全部领域一致——它由 options 决定是否生效，删掉它会让 3D 领域
// 的分支无处安放。

#include "domain/root_bindings.glsl"
#include "domain/checker_2d_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/provider/output_policy/output_policy_flat.glsl"
#include "module/provider/material_source/material_source_checkerboard_2d.glsl"
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
    HGLApplyAlpha(EvalAlpha(si, materialDataIndex));
#endif
    const vec4 finalColor =
        EvalPassEnvironment(PassEnvironmentInput(si, surface, materialDataIndex));
#if defined(HGL_ALPHA_OUTPUT)
    WriteMaterialOutput(vec4(finalColor.rgb, EvalAlpha(si, materialDataIndex)));
#else
    WriteMaterialOutput(HGLComposeColor(finalColor));
#endif
}
