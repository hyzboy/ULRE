#version 450
// ErrorChecker3D 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 错误回退棋盘格（黄/黑）：材质定义/几何/纹理任一校验失败时，ECS 渲染层把整条
// 配方换成该领域，用来「让问题看得见」。表面来源固定是
// `material/error_checkerboard_3d_source.glsl`。
//
// 与 Checker3D 的差别仅两处：
//   · 它**要**材质私有数据行（`fragDataIndexID`，location 0）——错误色与
//     闪烁相位随实例变化；
//   · 契约的 `material_private_data` 是 `GlobalSSBOType::EmissiveSurface`
//     （行跨度 16），不是 PBRSurface。
//
// 顶点侧同样要 `fragWorldPos`（location 1）并按屏幕导数补面法线兜底。

#include "domain/root_bindings.glsl"
#include "domain/emissive_row_bindings.glsl"
#include "generated/frag_options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/error_checkerboard_3d_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_WORLD_POS) in vec3 fragWorldPos;
#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL
layout(location=HGL_LOCATION_WORLD_NORMAL) in vec3 fragWorldNormal;
#endif
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
    si.viewDir = normalize(-fragWorldPos);
#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL
    si.worldNormal = normalize(fragWorldNormal);
#endif
#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK
    // 顶点流没有法线（本领域 normal_mode = OptionalFaceFallback，属于合法降级）。
    // 错误棋盘格必须**永远画得出来**，所以绝不能让它依赖几何是否带法线。
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
    const uint materialDataIndex = fragDataIndexID;
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
