#version 450

// === Compositor Template: Forward Unlit FS ===
// Unlit 渲染 — 不执行光照计算，直接输出 baseColor
// 适用于 SurfaceType::Unlit (PureColor3D, VertexColor3D 等)
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

layout(location=0) flat in uint fragMaterialInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
// 展开后: #include "surface/unlit_color3d_surface.glsl"

// --- MI SSBO ---
layout(set=2, binding=0) readonly buffer MaterialInstanceData { MI_Unlit mi_data[]; } mtl;

void main()
{
    MI_Unlit mi = mtl.mi_data[fragMaterialInstanceID];

    SurfaceInput si;
    si.worldPos    = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0         = vec2(0.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);

    SurfaceOutput so = EvalSurface(si, mi);

    // Unlit: 直接输出 baseColor，不做任何光照计算
    outColor = vec4(so.baseColor, so.alpha);
}
