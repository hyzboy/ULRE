// @ulre begin
// @ulre name unlit_2darray_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic UV0
// @ulre require ProducedSemantic MaterialData
// @ulre uses surface_interface
// @ulre ssbo mtl TextureRectArraySurface 0 Fragment optional fallback
// @ulre end

// Texture2DArray Surface — 从 si.uv0 的 2D 坐标 + material data 中的 layer 索引
// 采样 sampler2DArray 纹理直通输出。
// 不关心 UV 来自 2D NDC 还是 3D mesh。

#include "common/surface_interface.glsl"

layout(set=TEX_SET, binding=TEX_BINDING) uniform sampler2DArray TextureBaseColor;

#ifndef HGL_COVERAGE_ONLY
SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    TextureRectArraySurfaceData material_data = MTL_DATA.data[dataIndex];
    uint layer = material_data.id.x;

    vec4 texColor = texture(TextureBaseColor, vec3(si.uv0, float(layer)));

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
#endif

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    TextureRectArraySurfaceData material_data = MTL_DATA.data[dataIndex];
    uint layer = material_data.id.x;

    return texture(TextureBaseColor, vec3(si.uv0, float(layer))).a;
}
