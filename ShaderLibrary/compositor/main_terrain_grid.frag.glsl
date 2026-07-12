#version 450

// === Compositor Template: Terrain Grid FS ===
// 地形网格 — 从 VS 接收 clip-space 位置和世界法线
//
// Descriptor binding 约定：
//   Scene set=0 : camera=0, viewport=1

// Scene UBO (for specular half-vector)
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// Input from VS
layout(location=0) in vec4 fragClipPos;
layout(location=1) in vec3 fragWorldNormal;

layout(location=0) out vec4 outColor;

// Surface interface + surface function
#include "common/surface_interface.glsl"
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = fragClipPos.xyz;      // 原始代码传的是 clip-space position
    si.worldNormal = fragWorldNormal;
    si.uv0         = vec2(0.0);
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = 0.0;
    si.textureLayerID = 0u;

    SurfaceOutput so = EvalSurface(si, 0u);

    outColor = vec4(so.baseColor, so.alpha);
}
