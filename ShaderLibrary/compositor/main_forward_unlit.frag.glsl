// @ulre begin
// @ulre name main_forward_unlit
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic MaterialData
// @ulre end
#version 450

// === Compositor Template: Forward Unlit FS ===
// Unlit 渲染 — 不执行光照计算，直接输出 baseColor
// 适用于 SurfaceType::Unlit (PureColor3D 等)
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) flat in uint fragTextureLayerID;

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
    si.luminance   = 0.0;
    si.textureLayerID = fragTextureLayerID;

    SurfaceOutput so = EvalSurface(si, fragDataIndexID);

    // Unlit: 直接输出 baseColor，不做任何光照计算
    outColor = vec4(so.baseColor, so.alpha);
}
