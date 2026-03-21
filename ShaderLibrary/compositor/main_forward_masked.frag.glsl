#version 450


#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "common/varying_interface.glsl"

layout(location=0) out vec4 outColor;

#include SURFACE_FUNCTION_FILE

#ifndef ALPHA_THRESHOLD
#define ALPHA_THRESHOLD 0.5
#endif

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

    if (so.alpha < ALPHA_THRESHOLD) discard;

    outColor = vec4(so.baseColor, 1.0);
}
