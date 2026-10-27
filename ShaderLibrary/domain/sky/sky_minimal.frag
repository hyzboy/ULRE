#version 450
// Sky 域 · ForwardColor pass 的物理固定主着色器（无贴图大气表面）。
//
// SkyCube 的 cubemap 表面使用独立的 `sky.frag`。SkyMinimal 不需要材质贴图行，
// 必须直接接入 `sky_minimal_surface.glsl`，不能在无 texture_reference 的配方中
// 调用 `MTL_TEX`。

#include "domain/root_bindings.glsl"
#include "domain/sky_bindings.glsl"
#include "generated/frag_options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "sky/sky_atmosphere.glsl"
#include "common/surface_interface.glsl"
#include "surface/sky_minimal_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_FRAG_DIRECTION) in vec3 fragDirection;
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;
layout(location=0) out vec4 outColor;
void WriteMaterialOutput(vec4 value) { outColor = value * fragVertexColor; }

void main()
{
    SurfaceInput si;
    si.worldPos = fragDirection;
    si.worldNormal = normalize(fragDirection);
    si.uv0 = vec2(0.0);
    si.uv1 = vec2(0.0);
    si.vertexColor = fragVertexColor;
    si.viewDir = fragDirection;
    si.screenPos = vec2(0.0);
    si.luminance = 0.0;
    si.styleID = 0u;
    const SurfaceOutput so = EvalSurface(si, 0u);
    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));
}
