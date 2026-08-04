// @ulre begin
// @ulre name main_forward_unlit_luminance
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic Luminance
// @ulre require ProducedSemantic MaterialData
// @ulre end
#version 450

// === Compositor Template: Forward Unlit FS (Luminance) ===
// 顶点亮度 × MI 颜色 — 无光照，直接输出
//
// Descriptor binding 约定：
//   Scene    set=0 : (unused in FS)
//   Transform set=1 : (unused in FS)
//   Material set=2 : mtl=0 (MI SSBO, 由 surface function 声明)

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) flat in uint fragTextureLayerID;
layout(location=2) in float fragLuminance;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0         = vec2(0.0);
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(0.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = fragLuminance;
    si.textureLayerID = fragTextureLayerID;

    SurfaceOutput so = EvalSurface(si, fragDataIndexID);

    outColor = vec4(so.baseColor, so.alpha);
}
