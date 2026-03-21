#version 450


#include "common/ubo_camera.glsl"
#include "common/ubo_sky.glsl"
#include "common/surface_interface.glsl"

#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "common/varying_interface.glsl"

layout(location=0) out vec4 outColor;

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
