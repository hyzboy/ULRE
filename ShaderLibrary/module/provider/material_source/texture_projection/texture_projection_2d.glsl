// @ulre begin
// @ulre name texture_projection_2d
// @ulre kind Shared
// @ulre priority 0
// @ulre sampler_reference Trilinear
// @ulre uses bindless_textures
// @ulre end
// 材质源投影实现 — 普通 2D / 2DArray 贴图（默认）。
//
// 三份 `texture_projection_*.glsl` 提供**同名同签名**的 `SampleMaterialProjected`，
// 但它们共用同一个 include guard：整份程序里只会有一份生效。选中哪一份由
// `generated/frag_options.glsl` 末尾那行 `#include` 决定（见 DomainOptions /
// MaterialTextureProjection）——主着色器与 material_source_texture 都不出现
// 任何投影相关的 #if 分支。
//
// 本文件同时是"没有其它投影被选中"时的默认实现：material_source_texture 自己
// include 它，被 cube/八面体版本先占住 guard 时这行 include 退化为空操作。
#ifndef MATERIAL_TEXTURE_PROJECTION_GLSL
#define MATERIAL_TEXTURE_PROJECTION_GLSL

#include "module/descriptor/bindless_textures.glsl"

vec4 SampleMaterialProjected(uvec2 texture_reference, vec2 equirectUv)
{
    return Sample2DArray(texture_reference.x,
                         TrilinearSampler,
                         equirectUv,
                         float(texture_reference.y));
}

#endif // MATERIAL_TEXTURE_PROJECTION_GLSL
