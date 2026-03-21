#version 450


#define HAS_VERTEX_COLOR
#include "common/varying_interface.glsl"

layout(location=0) out vec4 outColor;

#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0         = vec2(0.0);
    si.uv1         = vec2(0.0);
    si.vertexColor = fragVertexColor;
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = 1.0;

    SurfaceOutput so = EvalSurface(si);  
    outColor = vec4(so.baseColor, so.alpha);
}
