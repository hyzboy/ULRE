// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ShadowInfo.h → ShadowCascadeInfo
//   布局：kind=struct bda=0 access=readwrite size=176 align=4 ref_align=16
//   说明：嵌套结构：被 ShadowInfo.cascades[4] 引用，自身不成块。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ShadowInfo.h → ShadowInfo
//   布局：kind=ubo bda=1 access=readonly size=832 align=4 ref_align=16
//   说明：按帧槽各一份（ring[i] 对应帧槽 i）。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_SHADOW_INFO_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_SHADOW_INFO_GLSL

struct ShadowCascadeInfo
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    vec4 cascade_params;
    vec4 cache_origin;
    uvec4 cache_offset;
    uvec4 cache_valid_rect;
};

struct ShadowInfoData
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    uvec4 csm_params;
    ShadowCascadeInfo cascades[4];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ShadowInfoRef
{
    mat4 shadow_vp;
    vec4 shadow_params;
    vec2 shadow_map_size;
    vec2 inv_shadow_map_size;
    uvec4 shadow_tex;
    uvec4 csm_params;
    ShadowCascadeInfo cascades[4];
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_UBO_SHADOW_INFO_GLSL
