#version 450
// StandardOpaque 域 · ShadowDepth pass（**含 alpha 覆盖判定**）的主着色器。
//
// 与 shadow_caster.frag 的唯一区别：这里真的采样材质 alpha 并 discard。
// 因此需要 PBR 的纹理表绑定、`<domain>_source`（提供 EvalMaterialAlpha）
// 与 ntb_none（`material_surface.glsl` 的解析闭包要求 GetNTB 可用，
// 即使 HGL_COVERAGE_ONLY 下 EvalSurface 本身被编译掉）。
//
// varyings 只有 fragDataIndexID + fragUV0：覆盖判定只需材质索引与 UV，
// 不需要世界坐标/法线。这个集合是 **StandardOpaque 专有**的——
// 例如 Checker3D 的同类 pass 用的是 worldPos+worldNormal，所以本文件
// 不能按"通用 masked 模板"来看待。
//
// 等价性：与 ShaderGen 旧路径产物 SPIR-V 尺寸全等、reflection 全等。

#include "domain/root_bindings.glsl"
#include "domain/standard_opaque_bindings.glsl"
#include "generated/frag_options.glsl"
#include "common/descriptor_macros.glsl"
#include "common/surface_interface.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/alpha_compositor.glsl"
#include "material/pbr_surface_source.glsl"
#include "ntb/ntb_none.glsl"
#include "surface/material_surface.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_UV0) in vec2 fragUV0;
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;

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
