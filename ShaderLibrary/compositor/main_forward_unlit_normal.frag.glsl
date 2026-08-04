// @ulre begin
// @ulre name main_forward_unlit_normal
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require Resource Camera
// @ulre require ProducedSemantic WorldPosition
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic MaterialData
// @ulre uses camera_info
// @ulre uses descriptor_macros
// @ulre end
#version 450

// === Compositor Template: Forward Unlit FS (with Normal + Camera) ===
// Unlit 输出（无框架光照），但提供 worldPos/worldNormal/camera 给 Surface Function
// 用于 Gizmo3D 等材质在 Surface Function 中做自定义光照
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

#include "common/descriptor_macros.glsl"
#include "ubo/camera_info.glsl"
SCENE_CAMERA_UBO;

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) flat in uint fragTextureLayerID;
layout(location=2) in vec3 fragWorldPos;
layout(location=3) in vec3 fragWorldNormal;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = vec2(0.0);
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(0.0);
    si.viewDir     = normalize(-fragWorldPos);   // camera-relative: camera at origin
    si.screenPos   = vec2(0.0);
    si.luminance   = 0.0;
    si.textureLayerID = fragTextureLayerID;

    SurfaceOutput so = EvalSurface(si, fragDataIndexID);

    // Unlit: 直接输出 Surface Function 的结果（光照由 Surface Function 内部处理）
    outColor = vec4(so.baseColor, so.alpha);
}
