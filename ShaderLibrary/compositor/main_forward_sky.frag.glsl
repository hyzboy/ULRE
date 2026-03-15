#version 450

// === Compositor Template: Forward Sky FS ===
// Procedural sky — 从表面函数获取天空颜色
//
// Descriptor binding 约定：
//   Scene set=0 : camera=0, sky=1, viewport=2
//   无 Material set (sky 无贴图/MI)

// Scene UBO — sky for procedural sky parameters
#include "common/scene_ubo.glsl"
SCENE_SKY_UBO(0, 1);

// Surface interface
#include "common/surface_interface.glsl"
#include SURFACE_FUNCTION_FILE

// Input from VS: sky direction
layout(location=0) in vec3 fragDirection;

layout(location=0) out vec4 outColor;

void main()
{
    SurfaceInput si;
    si.worldPos     = fragDirection;   // sky dome 里 worldPos 存放方向
    si.worldNormal  = vec3(0.0, 0.0, 1.0);
    si.uv0          = vec2(0.0);
    si.uv1          = vec2(0.0);
    si.vertexColor  = vec4(1.0);
    si.viewDir      = fragDirection;
    si.screenPos    = vec2(0.0);
    si.luminance    = 0.0;

    SurfaceOutput so = EvalSurface(si, 0u);

    outColor = vec4(so.baseColor, so.alpha);
}
