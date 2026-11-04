#version 450
// StandardAdvanced 域 · ShadowDepth pass（**含 alpha 覆盖判定**）的主着色器。
//
// 与 StandardOpaque 同名模板逐段同构，唯一差异是材质源换成
// material_source_pbr_advanced（64B 行），覆盖判定（行 alpha × opacity_mask）
// 语义完全一致。
//
// varyings 只有 fragDataIndexID + fragUV0：覆盖判定只需材质索引与 UV，
// 不需要世界坐标/法线。

#include "domain/root_bindings.glsl"
#include "domain/standard_advanced_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/interface/alpha_compositor.glsl"
#include "module/provider/material_source/material_source_pbr_advanced.glsl"
#include "module/provider/ntb/ntb_none.glsl"
#include "module/provider/surface/surface_material.glsl"
// 阶段间 varying 的输入声明来自 `module/stage/interstage_varying.glsl` 的片段段：
// 置位哪些语义由 `generated/common_options.glsl` 的 `HGL_VARYING_*` 决定，与网格
// 阶段读的是同一个宏集合，两端口径不可能分叉。派生量（viewDir / screenPos /
// 面法线兜底）仍在本文件里按领域赋值。
#define HGL_FRAGMENT_INPUT_VARYINGS
#include "module/stage/interstage_varying.glsl"

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
    si.uv0 = fragUV0;
    const uint materialDataIndex = fragDataIndexID;
    si.styleID = 0u;
    const float alpha = EvalAlpha(si, materialDataIndex);
    HGLApplyAlpha(alpha);
}
