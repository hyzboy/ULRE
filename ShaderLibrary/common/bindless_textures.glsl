// @ulre begin
// @ulre name bindless_textures
// @ulre kind Utility
// @ulre priority 0
// @ulre uses descriptor_macros
// @ulre end
// bindless_textures.glsl — 全局 Bindless 纹理数组（统一 Sampler 注册机制）
//
// 使用前须确保 descriptor_macros.glsl 已被 #include（提供 BINDLESS_SET）。
//
// 用法：
//   uint tex_handle = mtl_texture_layer_rows.data[dataIndex].base_color;
//   vec4 color      = Sample2D(tex_handle, TrilinearSampler, uv);
//
// tex_handle 为纯纹理句柄（1-based，0 = 无效），不再打包 sampler 下标。
// sampler 下标由 ShaderGen 以编译期宏字面量注入（如 "#define TrilinearSampler 2u"），
// 与运行时 binding=1 的 sampler 数组下标一一对应。
// Sample2D(0, ...) 返回 vec4(0)。
//
// 所有纹理（2D / 2DArray）统一注册为 texture2DArray[]（2D 为单层）；
// sampler 进独立 sampler[] 池（binding=1，由 SamplerPresetLibrary 按序注册）。
// TextureLayerRowsData 结构（含各 slot 字段）由 MaterialShaderCompiler 动态生成注入。

#ifndef BINDLESS_TEXTURES_GLSL
#define BINDLESS_TEXTURES_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

#include "descriptor_macros.glsl"

layout(set=BINDLESS_SET, binding=0) uniform texture2DArray bindless_tex[];
layout(set=BINDLESS_SET, binding=1) uniform sampler bindless_samp[];

// ── 采样辅助函数 ─────────────────────────────────────────────────────

vec4 Sample2D(uint tex_handle, uint samp_idx, vec2 uv)
{
    if (tex_handle == 0u)
        return vec4(0.0);
    return texture(sampler2DArray(bindless_tex[nonuniformEXT(tex_handle - 1u)],
                                  bindless_samp[nonuniformEXT(samp_idx)]),
                   vec3(uv, 0.0));
}

vec4 Sample2DArray(uint tex_handle, uint samp_idx, vec2 uv, float layer)
{
    if (tex_handle == 0u)
        return vec4(0.0);
    return texture(sampler2DArray(bindless_tex[nonuniformEXT(tex_handle - 1u)],
                                  bindless_samp[nonuniformEXT(samp_idx)]),
                   vec3(uv, layer));
}

#endif // BINDLESS_TEXTURES_GLSL
