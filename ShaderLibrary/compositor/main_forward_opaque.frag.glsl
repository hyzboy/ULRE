#version 450


layout(location=0) flat in uint fragMaterialInstanceID;
layout(location=1) in vec3 fragWorldPos;
layout(location=2) in vec3 fragWorldNormal;
layout(location=3) in vec2 fragUV0;

layout(location=0) out vec4 outColor;

#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#include SURFACE_FUNCTION_FILE

#include "common/lighting.glsl"

#include "common/ubo_camera.glsl"
#include "common/ubo_sky.glsl"
#include "common/ubo_viewport.glsl"
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
