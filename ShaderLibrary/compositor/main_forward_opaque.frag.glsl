#version 450

// === Compositor Template: Forward Opaque FS ===
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene    set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragDataIndexID;
layout(location=4) flat in uint fragTextureLayerID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
// 展开后类似: #include "surface/standard_surface.glsl"

// --- Lighting ---
#include "common/lighting.glsl"

// --- Scene Data (shared UBO definitions) ---
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_SKY_UBO;
SCENE_VIEWPORT_UBO;

void main()
{
    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(camera.pos - fragWorldPos);
    si.textureLayerID = fragTextureLayerID;

    SurfaceOutput so = EvalSurface(si, fragDataIndexID);

    vec3 litColor = EvalLighting(so, si.viewDir, sky.sun_direction.xyz, sky.sun_color.rgb);
    litColor += so.baseColor * sky.base_sky_color.rgb * so.ao;
    litColor += so.emissive;

    outColor = vec4(litColor, so.alpha);
}
