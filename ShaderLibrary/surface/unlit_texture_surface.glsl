// @ulre begin
// @ulre name unlit_texture_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require ProducedSemantic MaterialData
// @ulre uses surface_interface
// @ulre end

// Unlit Texture Surface — 通过 si.uv0 采样单一 2D 纹理直通输出。
// 纯数据源 surface，不关心传入的 UV 来源（2D NDC 或 3D mesh）。
// 纹理 sampler 声明使用 TEX_SET/TEX_BINDING，由 MaterialCompiler 注入。

#include "common/surface_interface.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2D TextureBaseColor;

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    vec4 texColor = texture(TextureBaseColor, si.uv0);

    SurfaceOutput so;
    so.baseColor = texColor.rgb;
    so.alpha     = texColor.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return texture(TextureBaseColor, si.uv0).a;
}
