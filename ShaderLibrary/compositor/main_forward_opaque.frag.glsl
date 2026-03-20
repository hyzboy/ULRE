#version 450


layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;

layout(location=0) out vec4 outColor;

#include SURFACE_FUNCTION_FILE

#include "common/lighting.glsl"

#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_SKY_UBO;
SCENE_VIEWPORT_UBO;


void main()
{
    MaterialInstance mi = GetMaterialInstance();

    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(camera.pos - fragWorldPos);

    SurfaceOutput so = EvalSurface(si);

    vec3 litColor = EvalLighting(so, si.viewDir, sky.sun_direction.xyz, sky.sun_color.rgb);
    litColor += so.baseColor * sky.base_sky_color.rgb * so.ao;
    litColor += so.emissive;

    outColor = vec4(litColor, so.alpha);
}
