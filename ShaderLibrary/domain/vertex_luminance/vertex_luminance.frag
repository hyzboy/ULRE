#version 450
// VertexLuminance 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 亮度来自顶点 varying（`material/luminance_source.glsl`），表面参数来自
// `EmissiveSurface` 材质私有数据行，所以本领域**需要** data index：
// `fragDataIndexID` 在 0、flat perprimitiveEXT。
//
// `fragLuminance` 落在 **location 7**（`MaterialVertexVaryingConfig` 的
// Luminance 槽位），是单个 float，不是向量。
//
// 面法线兜底块（`#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK`）**必须保留**：
// 本领域不做法线光照，但生成器对任何前向 pass 都无条件发射它，删掉会让
// SPIR-V 少 64 字节。别「推理掉」看着是死代码的东西。

#include "domain/root_bindings.glsl"
#include "domain/emissive_row_bindings.glsl"
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/luminance_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=0) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=7) in float fragLuminance;
layout(location=0) out vec4 outColor;
void WriteMaterialOutput(vec4 value) { outColor = value; }

void main()
{
    SurfaceInput si;
    si.worldPos = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0 = vec2(0.0);
    si.uv1 = vec2(0.0);
    si.vertexColor = vec4(1.0);
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
    si.luminance = fragLuminance;
    const uint materialDataIndex = fragDataIndexID;
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
