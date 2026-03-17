#version 450

// === Compositor Template: Forward Dither FS ===
// 有序抖动透明 — 用 4×4 Bayer 矩阵将连续 alpha 转化为二元 discard/keep。
// 适用于 LOD 过渡淡出、植被淡入淡出、无 AlphaBlend 的低端设备透明模拟。
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : (varies per surface function)
//
// Pipeline 要求：
//   - BlendEnable = VK_FALSE (无需混合，通过 discard 实现"透明")
//   - DepthWrite = On, DepthTest = GREATER_OR_EQUAL (Reversed-Z)
//   - 可以与 EarlyZ pre-pass 结合使用（pre-pass 也需要使用 dither FS）

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragMaterialInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE

// 4×4 Bayer ordered dither matrix，归一化到 [0/16, 15/16]
// 经典排列，产生 16 级均匀分布的屏幕空间抖动阈值
float BayerDither4x4(ivec2 p)
{
    const float bayer[16] = float[16](
         0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0,
        12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0,
         3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0,
        15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0
    );
    int idx = (p.y % 4) * 4 + (p.x % 4);
    return bayer[idx];
}

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

    // 根据屏幕像素坐标选取 Bayer 阈值，alpha 越大存活像素越多
    float threshold = BayerDither4x4(ivec2(gl_FragCoord.xy));
    if (so.alpha < threshold) discard;

    outColor = vec4(so.baseColor, 1.0);
}
