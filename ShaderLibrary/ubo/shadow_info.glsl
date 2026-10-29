// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ShadowInfo.h → ShadowCascadeInfo / ShadowInfo
//   布局：bda=1 access=readonly align=16
//   说明：ShadowCascadeInfo 是嵌套 GPU 结构（不成块），ShadowInfo 是块本体
//         （size=832，数组 cascades[4] 步长 176）。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_SHADOW_INFO_GLSL
#define HGL_SHADERLIB_UBO_SHADOW_INFO_GLSL

struct ShadowCascadeInfo
{
    mat4  shadow_vp;
    vec4  shadow_params;
    vec2  shadow_map_size;
    vec2  inv_shadow_map_size;
    uvec4 shadow_tex;
    vec4  cascade_params;
    vec4  cache_origin;
    uvec4 cache_offset;
    uvec4 cache_valid_rect;
};

struct ShadowInfoData
{
    mat4               shadow_vp;
    vec4               shadow_params;
    vec2               shadow_map_size;
    vec2               inv_shadow_map_size;
    uvec4              shadow_tex;
    uvec4              csm_params;
    ShadowCascadeInfo  cascades[4];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ShadowInfoRef
{
    mat4               shadow_vp;
    vec4               shadow_params;
    vec2               shadow_map_size;
    vec2               inv_shadow_map_size;
    uvec4              shadow_tex;
    uvec4              csm_params;
    ShadowCascadeInfo  cascades[4];
};

#endif // HGL_SHADERLIB_UBO_SHADOW_INFO_GLSL
