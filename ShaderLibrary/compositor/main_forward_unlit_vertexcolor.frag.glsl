#version 450

// === Compositor Template: Forward Unlit FS (Vertex Color) ===
// 直接输出顶点色，无 MI、无光照
//
// Descriptor binding 约定：
//   （FS 无额外 Descriptor）

layout(location=0) in vec4 fragVertexColor;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
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
    si.textureLayerID = 0u;

    SurfaceOutput so = EvalSurface(si, 0u);  // miID=0, 无 MI

    outColor = vec4(so.baseColor, so.alpha);
}
