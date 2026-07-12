#version 450

// === Compositor Template: Billboard FS ===
// Billboard 共用 FS — 采样 TextureBaseColor
// Dynamic 和 Fixed 共享此模板

layout(location=0) in vec2 fragTexCoord;

layout(location=0) out vec4 outColor;

// Surface interface + surface function
#include "common/surface_interface.glsl"
#include SURFACE_FUNCTION_FILE

void main()
{
    SurfaceInput si;
    si.worldPos    = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0         = fragTexCoord;
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = 0.0;
    si.textureLayerID = 0u;

    SurfaceOutput so = EvalSurface(si, 0u);

    outColor = vec4(so.baseColor, so.alpha);
}
