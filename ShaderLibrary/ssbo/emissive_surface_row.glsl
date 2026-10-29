// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MaterialDataRows.h → EmissiveSurfaceRow
//   布局：kind=ssbo bda=1 access=readwrite size=16 align=4 ref_align=16
//   说明：GlobalSSBOType::EmissiveSurface 的内容行；行跨度 16 字节 = 4 × 4。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_SSBO_EMISSIVE_SURFACE_ROW_GLSL
#define HGL_SHADERLIB_SSBO_EMISSIVE_SURFACE_ROW_GLSL

struct EmissiveSurfaceData
{
    vec4 color;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer EmissiveSurfaceRow
{
    vec4 color;
};

#endif // HGL_SHADERLIB_SSBO_EMISSIVE_SURFACE_ROW_GLSL
