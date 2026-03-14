#version 450

// === Compositor Template: Forward Unlit FS ===
// Unlit 渲染 — 不执行光照计算，直接输出 baseColor
// 适用于 SurfaceType::Unlit (PureColor3D, VertexColor3D 等)

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
// 展开后: #include "surface/unlit_color3d_surface.glsl"

// --- MI SSBO ---
layout(set=2, binding=0) readonly buffer MI_Buffer { MI_Unlit mi_data[]; };

void main()
{
    MI_Unlit mi = mi_data[fragInstanceID];

    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(-fragWorldPos);

    SurfaceOutput so = EvalSurface(si, mi);

    // Unlit: 直接输出 baseColor，不做任何光照计算
    outColor = vec4(so.baseColor, so.alpha);
}
