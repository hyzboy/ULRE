#version 450
// UnlitTexture 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 无光照：表面值直接走 flat_lighting 合成器。表面来源固定是
// `material/texture_source.glsl`（采样 base_color），这也是该领域契约钉死的
// 唯一一张图。
//
// `fragUV0` 落在 **location 3**（`MaterialVertexVaryingConfig` 的 UV0 槽位），
// `fragDataIndexID` 落在 0 且带 flat perprimitiveEXT：本领域**需要**材质私有
// 数据行（贴图引用就在行里），所以必须有 data index 输入。
//
// 必须保留 `#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK` 块（即使本领域不做任何
// 法线光照）：生成器对**任何前向 pass** 都无条件发射这块，它决定了
// `si.worldNormal` 是否被填值。删掉它 SPIR-V 会少 64 字节——等价性比对里
// 就是实打实的失败。别「推理掉」看着是死代码的东西。

#include "domain/root_bindings.glsl"
#include "domain/unlit_texture_bindings.glsl"
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/texture_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
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
    si.uv0 = fragUV0;
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
