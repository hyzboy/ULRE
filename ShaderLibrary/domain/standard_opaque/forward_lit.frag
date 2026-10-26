#version 450
// StandardOpaque 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 这是「废除 ShaderGen」后的终端形态：本文件是完整编译单元，直接交给
// glslang / GLSLCompiler.dll，没有模板占位、模块拼装或任何文本注入。
// 同一 domain 同一 pass 只有这一个文件；`.mi.toml`（MaterialRecipe）不上浮到
// 编译期，因此**改不了**本文件一个字——它只在运行时给 BDA 行填值。
//
// 唯一可变输入是 `generated/options.glsl`（只含 `#define` 与环境光 provider
// 的 include 行），选项词表见该文件。
//
// 等价性：SPIR-V 尺寸与 reflection 均已与 ShaderGen 旧路径产物全等。
// 改本文件前请先读 doc/shadergen-elimination-domain-fixed-architecture.md。

#include "domain/root_bindings.glsl"
#include "domain/standard_opaque_bindings.glsl"
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "ubo/scene_ubo.glsl"
#include "sky/sky_atmosphere.glsl"
#include "lighting/diffuse_lambert.glsl"
#include "lighting/specular_cook_torrance.glsl"
#include "shadow/pcf_shadow.glsl"
#include "ao/identity.glsl"
#include "lighting/forward_pbr.glsl"
#include "material/pbr_surface_source.glsl"
#include "ntb/ntb_tangent_vbo_normalmap.glsl"
#include "compositor/forward_lighting.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_WORLD_POS) in vec3 fragWorldPos;
layout(location=HGL_LOCATION_WORLD_NORMAL) in vec3 fragWorldNormal;
layout(location=HGL_LOCATION_UV0) in vec2 fragUV0;
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;
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
