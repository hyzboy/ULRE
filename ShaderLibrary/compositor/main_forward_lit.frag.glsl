#version 450

// === Compositor Template: Forward Lit FS ===
// Lit 材质共用片元模板 — BasicLit, PBRColor3D, TextureBlinnPhong
//
// 提供 camera + sky UBO，填充 SurfaceInput（world-space），
// 然后调用 SURFACE_FUNCTION_FILE 中的 EvalSurface()。
// 光照计算由各 surface function 自行完成（支持自定义光照模型）。

// Scene UBOs
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO(0, 0);
SCENE_SKY_UBO(0, 1);

// Surface interface
#include "common/surface_interface.glsl"

// Inputs from VS
layout(location=0) flat in uint fragMaterialInstanceID;
layout(location=1) in vec3 fragWorldPos;
layout(location=2) in vec3 fragWorldNormal;
layout(location=3) in vec2 fragUV0;

// Output
layout(location=0) out vec4 outColor;

// Surface function (injected by CompositorAssembler via #define SURFACE_FUNCTION_FILE)
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = normalize(-fragWorldPos); // camera at origin (camera-relative)
    si.screenPos   = gl_FragCoord.xy;
    si.luminance   = 0.0;

    SurfaceOutput so = EvalSurface(si, fragMaterialInstanceID);
    outColor = vec4(so.baseColor, so.alpha);
}
