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
// 等距柱状的方向编码 `MaterialDirectionToEquirectUv` 也放在这里（2D 的天空语义
// 就是经纬展开图），因此只 include 本文件的表面也能用方向版入口。
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

// 2D 语义的天空展开图 = 等距柱状（经纬）展开图：方向 → uv。
//
// 只有**天生拿不到几何 uv**的表面（Sky 域的解析天空模板，输入是 fragDirection
// 而不是 uv0）才需要它 —— 它服务于下面的方向变体。
vec2 MaterialDirectionToEquirectUv(vec3 direction)
{
    const float longitude = atan(direction.y, direction.x);
    const float latitude  = asin(clamp(direction.z, -1.0, 1.0));
    return vec2(longitude * 0.1591549430918953 + 0.5,
                latitude  * 0.3183098861837907 + 0.5);
}

// 第二形态的入口：直接吃方向，不要求调用方先编成 uv。
//
// 四份投影实现都提供同名同签名的本函数，语义统一为「这个方向上的贴图颜色」：
// 2D / 等距柱状先编 uv 再采样，cube 直接 SampleCubeArray，八面体直接编码。
// 这让拿得到方向的表面（Sky）不必付一次方向→uv→方向的往返量化；拿得到 uv 的
// 材质源路径继续走上面的 uv 版本，两边最终落在同一份被 options 选中的实现里。
vec4 SampleMaterialProjectedDirection(uvec2 texture_reference, vec3 direction)
{
    return SampleMaterialProjected(texture_reference,
                                   MaterialDirectionToEquirectUv(direction));
}

#endif // MATERIAL_TEXTURE_PROJECTION_GLSL
