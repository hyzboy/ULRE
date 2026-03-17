#version 450

// === Compositor Template: Forward Masked FS ===
// Alpha cutout — discards fragments whose alpha falls below ALPHA_THRESHOLD.
// 适用于植被、栅栏、镂空贴花等 AlphaMask 材质。
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : (varies per surface function)
//
// 注意：Pipeline 深度写入应保持启用（Opaque 模式），不需要 AlphaBlend。

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragMaterialInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE

#ifndef ALPHA_THRESHOLD
#define ALPHA_THRESHOLD 0.5
#endif

void main()
{
    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = normalize(-fragWorldPos); // camera-relative: cameraPos = 0
    si.screenPos   = gl_FragCoord.xy;
    si.luminance   = 0.0;

    SurfaceOutput so = EvalSurface(si, fragMaterialInstanceID);

    // Alpha cutout — discard before any write
    if (so.alpha < ALPHA_THRESHOLD) discard;

    // Output fully opaque (alpha=1.0 avoids blending artifacts with depth pre-pass)
    outColor = vec4(so.baseColor, 1.0);
}
