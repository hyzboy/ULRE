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

// --- Scene Data ---
layout(set=0, binding=0) uniform ViewportUBO { /* ... */ };
layout(set=0, binding=1) uniform CameraUBO { mat4 view; mat4 proj; mat4 viewProj; vec3 cameraPos; vec3 cameraPosWorld; };
layout(set=0, binding=2) uniform SkyUBO { vec3 sunDirection; vec3 sunColor; vec3 ambientColor; };

// --- MI SSBO ---
layout(set=2, binding=0) readonly buffer MI_Buffer { MI_Standard mi_data[]; };

void main()
{
    MI_Standard mi = mi_data[fragInstanceID];    // 简化——实际需要 TransformID → MI_ID 映射

    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(-fragWorldPos);  // cameraPos 恒为 0，故 viewDir = -worldPos

    SurfaceOutput so = EvalSurface(si, mi);

    vec3 litColor = EvalLighting(so, si.viewDir, sunDirection, sunColor);
    litColor += so.baseColor * ambientColor * so.ao;
    litColor += so.emissive;

    outColor = vec4(litColor, so.alpha);
}
