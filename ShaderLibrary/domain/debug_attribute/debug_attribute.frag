#version 450
// DebugAttribute 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 引擎内嵌调试视图：把中间量（世界法线 / 基础色 / 表面参数 / 自发光 / UV）
// 当颜色输出，供 AI Agent 与自动化测试做数值分析。设计见
// doc/debug-analysis-and-view-modes.md。
//
// 通道由 `generated/frag_options.glsl` 里的 `HGL_DEBUG_VIEW_*` 宏在**编译期**
// 选定（来源 = 配方的 compile_defines），本文件不含任何通道分支。
//
// 与 `debug_normal.frag` 的两处刻意差异：
//   1. 数据行绑定用 `standard_opaque_bindings.glsl`（PBRSurface，32B/行）——
//      调试配方**不搬动材质数据行**，读的就是被诊断材质原来那一行；
//   2. `si.vertexColor` 固定为白：`EvalSurface()` 会把 baseColor 乘上顶点色，
//      那会让通道值随几何染色而失真（分析要的是材质/几何事实，不是顶点色）。

#include "domain/root_bindings.glsl"
#include "domain/standard_opaque_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/provider/output_policy/output_policy_flat.glsl"
#include "module/provider/material_source/material_source_debug_attribute.glsl"
#include "module/provider/ntb/ntb_none.glsl"
#include "module/provider/surface/surface_material.glsl"
#include "module/interface/alpha_compositor.glsl"
// 阶段间 varying 的输入声明来自 `module/stage/interstage_varying.glsl` 的片段段：
// 置位哪些语义由 `generated/common_options.glsl` 的 `HGL_VARYING_*` 决定，与网格
// 阶段读的是同一个宏集合，两端口径不可能分叉。
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
    // 见文件头第 2 条：顶点色不参与调试通道。
    si.vertexColor = vec4(1.0);
    si.viewDir = vec3(0.0, 0.0, 1.0);
    si.screenPos = gl_FragCoord.xy;
    si.luminance = 1.0;
    si.styleID = 0u;
    si.worldPos = fragWorldPos;
    si.viewDir = normalize(camera.camera_world_pos - fragWorldPos);
    si.worldNormal = normalize(fragWorldNormal);
#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK
    // 面法线兜底必须排在顶点法线赋值**之后**：它要覆盖掉那条赋值。
    // 顶点格式没有 Normal 时，世界法线通道退化为几何法线 —— 这同样是有意义的
    // 分析对象（分析侧据此区分「顶点法线属性坏了」与「几何本身歪了」）。
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
    si.uv0 = fragUV0;
    const uint materialDataIndex = fragDataIndexID;
    si.styleID = 0u;
    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);
    // 调试通道不接受 alpha 覆盖：半透明会让相邻像素混色，破坏像素统计的可比性。
    const vec4 finalColor =
        EvalPassEnvironment(PassEnvironmentInput(si, surface, materialDataIndex));
#ifdef HGL_DEBUG_VIEW_WORLD_POSITION
    WriteMaterialOutput(vec4(si.worldPos, 1.0));
#else
    WriteMaterialOutput(HGLComposeColor(vec4(finalColor.rgb, 1.0)));
#endif
}
