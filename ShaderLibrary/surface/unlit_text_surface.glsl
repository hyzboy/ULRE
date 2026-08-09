// @ulre begin
// @ulre name unlit_text_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require ProducedSemantic MaterialData
// @ulre uses surface_interface
// @ulre ssbo mtl TransmissionSurface 0 Fragment optional fallback
// @ulre end

// Text Rendering Surface — 从 si.uv0 采样字体纹理（单通道 luminance），
// 与 material data 中的 TextColor 相乘后输出。
// 不关心 UV 来自 2D ortho 还是其他投影。

#include "common/surface_interface.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureText;

#ifndef HGL_COVERAGE_ONLY
SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    TransmissionSurfaceData material_data = MTL_DATA.data[dataIndex];
    vec4 textColor = unpackUnorm4x8(material_data.TextColor);
    float lum = texture(TextureText, si.uv0).r;

    SurfaceOutput so;
    so.baseColor = textColor.rgb * lum;
    so.alpha     = textColor.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}
#endif

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    TransmissionSurfaceData material_data = MTL_DATA.data[dataIndex];
    vec4 textColor = unpackUnorm4x8(material_data.TextColor);
    return textColor.a;
}
