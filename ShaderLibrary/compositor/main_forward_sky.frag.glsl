#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_SKY_UBO;

#include "common/surface_interface.glsl"
#include SURFACE_FUNCTION_FILE

layout(location=0) in vec3 fragDirection;

layout(location=0) out vec4 outColor;

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
