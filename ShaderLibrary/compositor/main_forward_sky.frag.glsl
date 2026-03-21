#version 450


#include "common/ubo_sky.glsl"
#include "common/surface_interface.glsl"

layout(location=0) flat in uint fragMaterialInstanceID;
layout(location=1) in vec3 fragDirection;

layout(location=0) out vec4 outColor;

#define MATERIAL_INSTANCE_ID_OVERRIDE fragMaterialInstanceID
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos     = fragDirection;       si.worldNormal  = vec3(0.0, 0.0, 1.0);
    si.uv0          = vec2(0.0);
    si.uv1          = vec2(0.0);
    si.vertexColor  = vec4(1.0);
    si.viewDir      = fragDirection;
    si.screenPos    = vec2(0.0);
    si.luminance    = 0.0;

    SurfaceOutput so = EvalSurface(si);

    outColor = vec4(so.baseColor, so.alpha);
}
