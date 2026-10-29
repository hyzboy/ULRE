// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/LightRow.h → LightRow
//   布局：kind=ssbo bda=1 access=readonly size=64 align=64 ref_align=16
//   说明：行步长 64 B 由 C++ 侧 alignas(64) + 显式 _pad 补齐；GLSL 的 scalar layout 不隐式补这个填充 ⇒ 末尾 _pad1[2] 必须保留。行数组下标 = 行号，主方向光恒为行 0。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_LIGHT_ROW_GLSL
#define HGL_SHADERLIB_UBO_LIGHT_ROW_GLSL

struct LightRowData
{
    uint type;
    uint flags;
    float _pad0[2];
    vec4 direction_intensity;
    vec4 color_angle;
    uint shadow_kind;
    uint shadow_index;
    uint _pad1[2];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer LightRowBufferRef
{
    LightRowData lights[];
};

#endif // HGL_SHADERLIB_UBO_LIGHT_ROW_GLSL
