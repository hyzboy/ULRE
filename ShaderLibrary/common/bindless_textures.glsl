// @ulre begin
// @ulre name bindless_textures
// @ulre kind Utility
// @ulre priority 0
// @ulre uses descriptor_macros
// @ulre end
// bindless_textures.glsl — 全局 Bindless 纹理数组
//
// 使用前须确保 descriptor_macros.glsl 已被 #include（提供 BINDLESS_SET）。
//
// 用法：
//   uint handle = mtl_texture_layer_rows.data[dataIndex].base_color;
//   vec4 color  = SampleBindless2D(handle, uv);
//
// handle 为 1-based（0 = 无效）；SampleBindless2D(0, uv) 返回 vec4(0)。
// 所有纹理（2D / 2DArray）统一注册为 2DArray 数组；2D 纹理为单层。
// TextureLayerRowsData 结构（含各 slot 字段）由 MaterialShaderCompiler 动态生成注入。

#ifndef BINDLESS_TEXTURES_GLSL
#define BINDLESS_TEXTURES_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

#include "descriptor_macros.glsl"

layout(set=BINDLESS_SET, binding=0) uniform sampler2DArray bindless_tex2darray[];

// ── 采样辅助函数 ─────────────────────────────────────────────────────

vec4 SampleBindless2D(uint handle, vec2 uv)
{
    if (handle == 0u)
        return vec4(0.0);
    return texture(bindless_tex2darray[nonuniformEXT(handle - 1u)], vec3(uv, 0.0));
}

vec4 SampleBindless2DArray(uint handle, vec2 uv, float layer)
{
    if (handle == 0u)
        return vec4(0.0);
    return texture(bindless_tex2darray[nonuniformEXT(handle - 1u)], vec3(uv, layer));
}

#endif // BINDLESS_TEXTURES_GLSL
