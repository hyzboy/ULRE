#version 450

// === Compositor Template: Forward Transparent FS ===
// Alpha-blended transparency — passes fragment alpha from SurfaceOutput through.
// 适用于玻璃、半透明粒子、透明玻璃片等材质。
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : (varies per surface function)
//
// Pipeline 要求：
//   - BlendEnable = VK_TRUE
//   - srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA
//   - dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
//   - DepthWrite = Off, DepthTest = Less（Reversed-Z: GREATER_OR_EQUAL）
//   - 按 distance 从后往前排序绘制（painter's algorithm）

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragMaterialInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE

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

    // Pass through with full alpha — pipeline blends against existing framebuffer content
    outColor = vec4(so.baseColor, so.alpha);
}
