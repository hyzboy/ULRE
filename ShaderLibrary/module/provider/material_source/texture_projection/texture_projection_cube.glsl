// @ulre begin
// @ulre name texture_projection_cube
// @ulre kind Shared
// @ulre priority 0
// @ulre sampler_reference Trilinear
// @ulre uses bindless_textures
// @ulre uses texture_projection_equirect
// @ulre end
// 材质源投影实现 — Cubemap（`*.TexCube`）。
//
// 与 2D 版本同名同签名、共享同一个 include guard：由 frag_options 的那行
// `#include` 选中本文件后，2D 默认实现就再也进不来，因此整份程序里
// `SampleMaterialProjected` 只有一个定义，主着色器无需 #if。
//
// 采样语义：把球体几何的等距柱状 uv 还原成方向，交给 samplerCubeArray。
// 单张贴图即单张 cubemap，array layer 恒为 0（句柄本身已经定位到那张 cubemap
// 的 6 层视图）。
#ifndef MATERIAL_TEXTURE_PROJECTION_GLSL
#define MATERIAL_TEXTURE_PROJECTION_GLSL

#include "module/provider/material_source/texture_projection/texture_projection_equirect.glsl"

// 方向直接入参的变体：Sky 域已经握有 fragDirection，不必往返编码。
vec4 SampleMaterialProjectedDirection(uvec2 texture_reference, vec3 direction)
{
    return SampleCubeArray(texture_reference.x,
                           TrilinearSampler,
                           vec4(direction, 0.0));
}

vec4 SampleMaterialProjected(uvec2 texture_reference, vec2 equirectUv)
{
    return SampleMaterialProjectedDirection(
        texture_reference, MaterialEquirectUvToDirection(equirectUv));
}

#endif // MATERIAL_TEXTURE_PROJECTION_GLSL
