#version 450

// === Compositor Template: Forward Alpha-to-Coverage FS ===
// Alpha-to-Coverage (A2C) — 将 fragment alpha 转换为 MSAA 采样点覆盖率。
// 适用于高质量植被、头发等需要抗锯齿边缘的 cutout 材质。
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : (varies per surface function)
//
// Pipeline 要求：
//   - multisampleStateInfo.alphaToCoverageEnable = VK_TRUE
//   - MSAA 采样数 >= 2x（推荐 4x），否则 A2C 退化为普通 Masked
//   - BlendEnable = VK_FALSE（A2C 与 AlphaBlend 不兼容）
//   - DepthWrite = On, DepthTest = GREATER_OR_EQUAL (Reversed-Z)
//
// 注意：与 Masked 的区别在于边缘质量。A2C 利用 MSAA 亚像素覆盖实现平滑边缘，
//       Masked 仅做 discard，边缘为像素级锯齿。

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

    // Alpha 传递给 pipeline 做 alpha-to-coverage 转换。
    // GPU 根据 alpha 值决定写入哪些 MSAA 采样点，不走 AlphaBlend 路径。
    outColor = vec4(so.baseColor, so.alpha);
}
