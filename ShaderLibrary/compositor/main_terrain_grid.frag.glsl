#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

layout(location=0) in vec4 fragClipPos;
layout(location=1) in vec3 fragWorldNormal;

layout(location=0) out vec4 outColor;

#include "common/surface_interface.glsl"
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = fragClipPos.xyz;          si.worldNormal = fragWorldNormal;
    si.uv0         = vec2(0.0);
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = 0.0;

    SurfaceOutput so = EvalSurface(si);

    outColor = vec4(so.baseColor, so.alpha);
}
