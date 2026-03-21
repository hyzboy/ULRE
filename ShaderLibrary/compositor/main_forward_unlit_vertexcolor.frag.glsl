#version 450


layout(location=0) flat in uint fragMaterialInstanceID;
layout(location=1) in vec4 fragVertexColor;

layout(location=0) out vec4 outColor;

#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
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
