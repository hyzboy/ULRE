// @ulre begin
// @ulre name main_forward_unlit_texture
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require ProducedSemantic MaterialData
// @ulre end
#version 450

// === Compositor Template: Forward Unlit FS (with UV - Texture) ===
// Unlit 纹理采样渲染 — surface 通过 si.uv0 采样纹理直通输出。
// 不关心 UV 来自 2D NDC 投影还是 3D mesh，统一支持。
// 无光照计算。MaterialData 用于 per-instance SSBO 数据（可选）。

layout(location=0) flat in uint fragDataIndexID;
layout(location=1) flat in uint fragTextureLayerID;
layout(location=2) in vec2 fragUV0;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
#include "common/alpha_compositor.glsl"

void main()
{
    SurfaceInput si;
    si.worldPos    = vec3(0.0);
    si.worldNormal = vec3(0.0, 0.0, 1.0);
    si.uv0         = fragUV0;
    si.uv1         = vec2(0.0);
    si.vertexColor = vec4(1.0);
    si.viewDir     = vec3(0.0, 0.0, 1.0);
    si.screenPos   = vec2(0.0);
    si.luminance   = 1.0;
    si.textureLayerID = fragTextureLayerID;

    SurfaceOutput so = EvalSurface(si, fragDataIndexID);

    // Unlit: 直接输出 baseColor，不做任何光照计算
    outColor = HGLComposeColor(vec4(so.baseColor, so.alpha));
}
