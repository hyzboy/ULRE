#version 450

// === Compositor Template: Forward Opaque FS ===

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
// 展开后类似: #include "surface/standard_surface.glsl"

// --- Lighting ---
#include "common/lighting.glsl"

// --- Scene Data (shared UBO definitions) ---
#include "common/scene_ubo.glsl"
SCENE_VIEWPORT_UBO(0, 0);
SCENE_CAMERA_UBO(0, 1);
SCENE_SKY_UBO(0, 2);

// --- MI SSBO ---
layout(set=2, binding=0) readonly buffer MI_Buffer { MI_Standard mi_data[]; };

void main()
{
    MI_Standard mi = mi_data[fragInstanceID];    // 简化——实际需要 TransformID → MI_ID 映射

    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(camera.pos - fragWorldPos);

    SurfaceOutput so = EvalSurface(si, mi);

    vec3 litColor = EvalLighting(so, si.viewDir, sky.sun_direction.xyz, sky.sun_color.rgb);
    litColor += so.baseColor * sky.base_sky_color.rgb * so.ao;
    litColor += so.emissive;

    outColor = vec4(litColor, so.alpha);
}
