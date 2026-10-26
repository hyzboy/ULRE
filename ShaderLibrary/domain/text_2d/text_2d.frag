#version 450
// Text2D 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 字形四边形（mesh 阶段 s1_text_char_quad 出几何）：表面来源固定是
// `material/text_source_gpu.glsl`，它按 `TEXT_SDF_ENABLED` 走 SDF 距离场
// 或原始位图两条**既存分支**——该宏来自材质定义的 `defines`，由
// options.glsl 发放，不在本文件里判断。
//
// 输入比其它领域多一条 `fragStyleID`（**location 4**，flat perprimitiveEXT）：
// 它索引 `sbo_char_style` 样式表（加粗/勾边/阴影参数），是该领域独有的
// 契约要求。`fragDataIndexID`（0）指向贴图表行，`fragUV0`（3）是图集 UV，
// `fragVertexColor`（5）是文字颜色。
//
// ⚠ 本领域定义了 `HGL_ALPHA_OUTPUT`（`alpha_blend = true`）：SRC_ALPHA 必须取
// 输出 alpha（预乘后的覆盖率），不能写光照 alpha。

#include "domain/root_bindings.glsl"
#include "domain/text_2d_bindings.glsl"
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/text_source_gpu.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_UV0) in vec2 fragUV0;
layout(location=HGL_LOCATION_STYLE_ID) flat perprimitiveEXT in  uint fragStyleID;
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
    si.vertexColor = vec4(1.0);
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
    si.vertexColor = fragVertexColor;
    const uint materialDataIndex = fragDataIndexID;
    si.styleID = fragStyleID;
    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);
    const vec4 finalColor =
        EvalPassEnvironment(PassEnvironmentInput(si, surface, materialDataIndex));
    // ⚠ 本领域的尾段**没有** `#if defined(HGL_ALPHA_OUTPUT)` 分支，也没有独立的
    // `HGLApplyAlpha(EvalAlpha(...))` 块——它与 UnlitTexture 同走「unlit」骨架，
    // 那条骨架只写 `HGLComposeColor(finalColor)`：
    //   · 覆盖判定由 `HGLComposeColor` 内部的 `HGLApplyAlpha(color.a)` 完成；
    //   · `alpha_blend` 需要的 alpha 由 flat 合成器把 `surface.alpha` 带进
    //     `finalColor.a`（文本源已预乘，正是 SRC_ALPHA 要的值）。
    // 顺手「补上」那两段会让 SPIR-V 变大 1360 字节，等价性比对直接失败。
    WriteMaterialOutput(HGLComposeColor(finalColor));
}
