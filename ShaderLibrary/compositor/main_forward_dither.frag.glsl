#version 450


#define HAS_WORLD_POS
#define HAS_WORLD_NORMAL
#define HAS_UV0
#include "common/varying_interface.glsl"

layout(location=0) out vec4 outColor;

#include SURFACE_FUNCTION_FILE

float BayerDither4x4(ivec2 p)
{
    const float bayer[16] = float[16](
         0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0,
        12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0,
         3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0,
        15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0
    );
    int idx = (p.y % 4) * 4 + (p.x % 4);
    return bayer[idx];
}

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

    float threshold = BayerDither4x4(ivec2(gl_FragCoord.xy));
    if (so.alpha < threshold) discard;

    outColor = vec4(so.baseColor, 1.0);
}
