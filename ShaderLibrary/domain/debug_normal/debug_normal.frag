#version 450
// DebugNormal 域 · ForwardColor pass 的**物理固定**主着色器。
//
// 调试用：把世界法线直接当颜色输出。表面来源固定是
// `material/debug_normal_source.glsl`，它读材质私有行取调试色权，
// 所以契约的 `material_private_data` 是 `GlobalSSBOType::EmissiveSurface`。
//
// 本领域是**法线必需**的领域（`normal_mode` 要求顶点法线）：
// `HGL_MATERIAL_HAS_VERTEX_NORMAL` 由 options.glsl 发放，主着色器据此把
// `si.worldNormal = normalize(fragWorldNormal)`（location 2）。面法线兜底块
// 仍保留：ECS 层的错配回退可能让构建请求失去 Normal 语义，那时代码必须能编译、
// 且画出来的东西同样「看得出是法线」。

#include "domain/root_bindings.glsl"
#include "domain/emissive_row_bindings.glsl"
#include "generated/options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/surface_interface.glsl"
#include "common/pass_environment_interface.glsl"
#include "compositor/flat_lighting.glsl"
#include "material/debug_normal_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_WORLD_POS) in vec3 fragWorldPos;
layout(location=HGL_LOCATION_WORLD_NORMAL) in vec3 fragWorldNormal;
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
    si.worldPos = fragWorldPos;
    si.viewDir = normalize(-fragWorldPos);
    si.worldNormal = normalize(fragWorldNormal);
#ifdef HGL_MATERIAL_FACE_NORMAL_FALLBACK
    // 面法线兜底必须排在顶点法线赋值**之后**：它要覆盖掉那条赋值。
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
