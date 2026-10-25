#version 450
// VertexColor / VertexPalette 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 表面色 100% 来自顶点色（`material/vertex_color_source.glsl`），因此：
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
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/vertex_color_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=5) in vec4 fragVertexColor;
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
    si.vertexColor = fragVertexColor;
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
