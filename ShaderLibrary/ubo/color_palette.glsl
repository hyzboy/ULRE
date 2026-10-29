// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/ColorPalette.h → ColorPalette
//   布局：kind=ubo bda=1 access=readonly size=1024 align=4 ref_align=16
//   说明：256 项 RGBA8 打包（unpackUnorm4x8 解码），构造期一次写入、长期有效。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_UBO_COLOR_PALETTE_GLSL
#define HGL_SHADERLIB_UBO_COLOR_PALETTE_GLSL

struct ColorPaletteData
{
    uint color[256];
};

layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer ColorPaletteRef
{
    uint color[256];
};

#endif // HGL_SHADERLIB_UBO_COLOR_PALETTE_GLSL
