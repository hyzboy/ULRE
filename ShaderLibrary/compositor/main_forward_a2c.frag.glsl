#version 450


layout(location=0) flat in uint fragMaterialInstanceID;
layout(location=1) in vec3 fragWorldPos;
layout(location=2) in vec3 fragWorldNormal;
layout(location=3) in vec2 fragUV0;

layout(location=0) out vec4 outColor;

#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = normalize(-fragWorldPos);     si.screenPos   = gl_FragCoord.xy;
    si.luminance   = 0.0;

    SurfaceOutput so = EvalSurface(si);

    outColor = vec4(so.baseColor, so.alpha);
}
