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
//   可选槽（句柄 0 = 未绑定，fallback 保底，乘性槽用 vec4(1.0)）：
//     vec4 color = SampleOptional(MTL_TEX(dataIndex).tex_base_color, TrilinearSampler, uv, vec4(1.0));
//   已由 required 保证绑定、或需要显式层号的槽：
//     vec4 color = Sample2DArray(tex_handle, TrilinearSampler, uv, layer);
//
// tex_handle 为纯纹理句柄（1-based，0 = 无效），不再打包 sampler 下标。
// sampler 下标由 ShaderGen 以编译期宏字面量注入（如 "#define TrilinearSampler 2u"），
// 与运行时 binding=1 的 sampler 数组下标一一对应。
// Sample2D(0, ...) 返回 vec4(0)。
//
// 所有纹理（2D / 2DArray）统一注册为 texture2DArray[]（2D 为单层）；
// sampler 进独立 sampler[] 池（binding=1，由 SamplerPresetLibrary 按序注册）。
// MTL_TEX() returns the MaterialDefinition-specific uvec2 reference row:
// .x is the bindless descriptor index and .y is the Texture2DArray layer.

#ifndef BINDLESS_TEXTURES_GLSL
#define BINDLESS_TEXTURES_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

#include "descriptor_macros.glsl"

layout(set=BINDLESS_SET, binding=0) uniform texture2DArray bindless_tex[];
layout(set=BINDLESS_SET, binding=1) uniform sampler bindless_samp[];
// Cubemap 纹理数组（与 binding=0 共享 1-based handle 空间，按材质槽类型分流）
layout(set=BINDLESS_SET, binding=2) uniform textureCube bindless_cube[];

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

// Cubemap 方向采样（handle 与 2D 纹理共享编号空间）
vec4 SampleCube(uint tex_handle, uint samp_idx, vec3 dir)
{
    if (tex_handle == 0u)
        return vec4(0.0);
    return texture(samplerCube(bindless_cube[nonuniformEXT(tex_handle - 1u)],
                               bindless_samp[nonuniformEXT(samp_idx)]),
                   dir);
}

// 可选纹理槽统一取样：引用行句柄为 0（未绑定）时返回 fallback，
// 否则按 (handle, sampler, uv, layer) 取样。
// 与手写的 "if (ref.x != 0u) { ... Sample2DArray(ref.x, ...) }" 语义等价，
// 但把守卫与取样收口到一处，材质源模块里每个槽只剩一行。
vec4 SampleOptional(uvec2 tex_ref, uint samp_idx, vec2 uv, vec4 fallback)
{
    return tex_ref.x == 0u
        ? fallback
        : Sample2DArray(tex_ref.x, samp_idx, uv, float(tex_ref.y));
}

#endif // BINDLESS_TEXTURES_GLSL
