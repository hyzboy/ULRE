#version 450
// StandardOpaque 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 这是「废除 ShaderGen」后的终端形态：本文件是完整编译单元，直接交给
// glslang / GLSLCompiler.dll，没有模板占位、模块拼装或任何文本注入。
// 同一 domain 同一 pass 只有这一个文件；`.mi.toml`（MaterialRecipe）不上浮到
// 编译期，因此**改不了**本文件一个字——它只在运行时给 BDA 行填值。
//
// 唯一可变输入是 `generated/frag_options.glsl`（只含 `#define` 与环境光 provider
// 的 include 行），选项词表见该文件。
//
// 等价性：SPIR-V 尺寸与 reflection 均已与 ShaderGen 旧路径产物全等。
// 改本文件前请先读 doc/shadergen-elimination-domain-fixed-architecture.md。

#include "domain/root_bindings.glsl"
#include "domain/standard_opaque_bindings.glsl"
#include "generated/frag_options.glsl"
#include "contract/descriptor_macros.glsl"
#include "module/interface/surface_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/provider/sky/sky_atmosphere.glsl"
#include "module/provider/diffuse_light/diffuse_light_lambert.glsl"
#include "module/provider/specular_light/specular_light_cook_torrance.glsl"
#include "module/provider/shadow/shadow_pcf.glsl"
#include "module/provider/ambient_occlusion/ambient_occlusion_identity.glsl"
#include "module/provider/lighting_model/lighting_model_pbr.glsl"
#include "module/provider/material_source/material_source_pbr_surface.glsl"
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
