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
//   uint handle = mtl_texture_layer_rows.values[TEXTURE_SLOT_BASE_COLOR];
//   vec4 color  = SampleBindless2D(handle, uv);
//
// handle 为 1-based（0 = 无效）；SampleBindless2D(0, uv) 返回 vec4(0)。

#ifndef BINDLESS_TEXTURES_GLSL
#define BINDLESS_TEXTURES_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

#include "descriptor_macros.glsl"

layout(set=BINDLESS_SET, binding=0) uniform sampler2D      bindless_tex2d[];
layout(set=BINDLESS_SET, binding=1) uniform sampler2DArray bindless_tex2darray[];

// ── 采样辅助函数 ─────────────────────────────────────────────────────

vec4 SampleBindless2D(uint handle, vec2 uv)
{
    if (handle == 0u)
        return vec4(0.0);
    return texture(bindless_tex2d[nonuniformEXT(handle - 1u)], uv);
}

vec4 SampleBindless2DLod(uint handle, vec2 uv, float lod)
{
    if (handle == 0u)
        return vec4(0.0);
    return textureLod(bindless_tex2d[nonuniformEXT(handle - 1u)], uv, lod);
}

vec4 SampleBindless2DArray(uint handle, vec2 uv, float layer)
{
    if (handle == 0u)
        return vec4(0.0);
    return texture(bindless_tex2darray[nonuniformEXT(handle - 1u)], vec3(uv, layer));
}

vec4 FetchBindless2D(uint handle, ivec2 coord, int lod)
{
    if (handle == 0u)
        return vec4(0.0);
    return texelFetch(bindless_tex2d[nonuniformEXT(handle - 1u)], coord, lod);
}

// ── TextureSlot 枚举常量（与 C++ TextureSlot 保持一致） ─────────────

#define TEXTURE_SLOT_BASE_COLOR   0u
#define TEXTURE_SLOT_NORMAL       1u
#define TEXTURE_SLOT_METALLIC     2u
#define TEXTURE_SLOT_ROUGHNESS    3u
#define TEXTURE_SLOT_EMISSIVE     4u
#define TEXTURE_SLOT_OCCLUSION    5u
#define TEXTURE_SLOT_OPACITY_MASK 6u
#define TEXTURE_SLOT_HEIGHT       7u
#define TEXTURE_SLOT_CUSTOM0      8u
#define TEXTURE_SLOT_CUSTOM1      9u

// 纹理槽数量（必须与 C++ 的 graph::mtl::TextureSlot::RANGE_SIZE 保持一致）。
// 正常路径由 ShaderGen/MaterialCompiler 以 TextureSlot::RANGE_SIZE 注入该宏；
// 这里保留默认值作为兜底。
#ifndef TEXTURE_SLOT_RANGE_SIZE
#define TEXTURE_SLOT_RANGE_SIZE 10u
#endif

// 别名宏，便于旧 shader 逐步收口到 TEXTURE_SLOT_RANGE_SIZE。
#define TEXTURE_SLOT_COUNT TEXTURE_SLOT_RANGE_SIZE

// 取指定 instance row 和槽位的 bindless handle
// iid：textureLayerID（=gl_InstanceIndex，即行索引）
// slot：TextureSlot 枚举整数常量
#define GetTextureHandle(iid, slot) \
    mtl_texture_layer_rows.values[(iid) * TEXTURE_SLOT_RANGE_SIZE + (slot)]

// 便利采样宏（需要 mtl_texture_layer_rows 已声明）
#define SAMPLE_BINDLESS_SLOT_2D(iid, slot, uv) \
    SampleBindless2D(GetTextureHandle((iid), (slot)), (uv))

#define SAMPLE_BINDLESS_SLOT_2DARRAY(iid, slot, uv, layer) \
    SampleBindless2DArray(GetTextureHandle((iid), (slot)), (uv), (layer))

#endif // BINDLESS_TEXTURES_GLSL
