#version 450
// Checker3D 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 3D 棋盘格：表面来源固定是 `module/provider/material_source/material_source_checkerboard_3d.glsl`，图案按
// **世界坐标**算，所以本领域要 `fragWorldPos`（location 1）并同时刷新
// `si.viewDir = normalize(-worldPos)`（相机在视空间原点，-worldPos 就是视线）。
// 它同样不读材质私有数据行、不读贴图 ⇒ `materialDataIndex` 是字面 `0u`。
//
// 该模块自己也有 `#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL` 分支（有顶点法线就
// 直接用，没有就用下面的面法线兜底）；两个宏由 frag_options.glsl 按构建请求发放，
// 本文件不判断。

#include "domain/root_bindings.glsl"
#include "domain/checker_3d_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/provider/output_policy/output_policy_flat.glsl"
#include "module/provider/material_source/material_source_checkerboard_3d.glsl"
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
    si.worldPos = fragWorldPos;
    si.viewDir = normalize(-fragWorldPos);
#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL
    si.worldNormal = normalize(fragWorldNormal);
#endif
#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK
    // 顶点流没有法线（本领域 normal_mode = OptionalFaceFallback，属于合法降级）。
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
