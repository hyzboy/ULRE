// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MaterialDataRows.h → PBRSurfaceRow
//   布局：kind=ssbo bda=1 access=readwrite size=32 align=4 ref_align=16
//   说明：GlobalSSBOType::PBRSurface 的内容行；行跨度 32 字节 = 8 × 4。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_SSBO_PBR_SURFACE_ROW_GLSL
#define HGL_SHADERLIB_SSBO_PBR_SURFACE_ROW_GLSL

struct PBRSurfaceData
{
    vec4 base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer PBRSurfaceRow
{
    vec4 base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
};

#endif // HGL_SHADERLIB_SSBO_PBR_SURFACE_ROW_GLSL
