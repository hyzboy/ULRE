#version 450
// StandardAdvanced 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 与 StandardOpaque/forward_lit.frag 的差异只有一处思想：高级直接光槽的
// **基线绑定不再是空对象**，而是 filament 实现（清漆 GGX 双层 / 绒面 Charlie /
// 简易次表面 Wrap / 各向异性高光替换基础 GGX）。参数为 0 时各项加法归零、
// 乘性项归一，数学上退化回标准 Lit——数据决定行为，不再有第二条编译分支。
//
// 其余结构与标准域逐段同构（废除 ShaderGen 后的终端形态，无模板占位、
// 无文本注入；改本文件前先读 doc/shadergen-elimination-domain-fixed-architecture.md）。

#include "domain/root_bindings.glsl"
#include "domain/standard_advanced_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/provider/sky/sky_atmosphere.glsl"
#include "module/provider/diffuse_light/diffuse_light_lambert.glsl"
// 高光槽：各向异性 GGX（anisotropy=0 时数学退化为标准 GGX）。
#include "module/provider/specular_light/specular_light_anisotropic.glsl"
// 高级直接光三槽：filament 实现（本域的存在理由）。
// include 顺序与 PassProfile.cpp 的 ForwardLitSlots 表一致。
#include "module/provider/clearcoat_light/clearcoat_light_ggx.glsl"
#include "module/provider/sheen_light/sheen_light_charlie.glsl"
#include "module/provider/subsurface_light/subsurface_light_wrap.glsl"
#include "module/provider/shadow/shadow_pcf.glsl"
#include "module/provider/ambient_occlusion/ambient_occlusion_identity.glsl"
#include "module/provider/lighting_model/lighting_model_pbr.glsl"
#include "module/provider/material_source/material_source_pbr_advanced.glsl"
#include "module/provider/ntb/ntb_tangent_vbo_normalmap.glsl"
#include "module/provider/output_policy/output_policy_forward.glsl"
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
    si.viewDir = normalize(camera.camera_world_pos - fragWorldPos);
    si.worldNormal = normalize(fragWorldNormal);
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
    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);
#if defined(HGL_ALPHA_TEST) || defined(HGL_ALPHA_DITHER)
    // 覆盖（coverage）材质：按材质源的 alpha 槽丢弃片元（与 ShadowCasterMasked 的
    // EvalAlpha 同语义——opacity_mask 未绑定时 SampleOptional 保底 1.0）。
    // 光照输出的 alpha 恒为 1，不能用作覆盖判定。
    // ⚠ 守卫必须同时覆盖 HGL_ALPHA_DITHER：dither 由本节 HGLApplyAlpha 内部
    // （alpha_compositor.glsl 的 HGL_ALPHA_DITHER 分支）实现，只判 HGL_ALPHA_TEST
    // 会把 dither 材质的覆盖判定整个编译掉 ⇒ dither 静默失效（cutoff/blend 同理看 alpha）。
    HGLApplyAlpha(EvalAlpha(si, materialDataIndex));
#endif
    const vec4 finalColor = EvalPassEnvironment(PassEnvironmentInput(si, surface, materialDataIndex));
#if defined(HGL_ALPHA_OUTPUT)
    // 曲面 alpha 写进输出：`alpha_blend` 的 SRC_ALPHA 取**输出 alpha**。
    // 此前恒写 HGLComposeColor(finalColor)（alpha = 光照 alpha = 1）⇒ blend 空转
    //（等价不透明）。覆盖判定（discard）已在上方执行。
    WriteMaterialOutput(vec4(finalColor.rgb, EvalAlpha(si, materialDataIndex)));
#else
    WriteMaterialOutput(HGLComposeColor(finalColor));
#endif
}
