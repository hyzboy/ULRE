// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MaterialDataRows.h → PBRAdvancedSurfaceRow
//   布局：kind=ssbo bda=1 access=readwrite size=64 align=4 ref_align=16
//   说明：GlobalSSBOType::PBRAdvancedSurface 的内容行（Filament 高级材质扩展）；前 32B 与 PBRSurfaceRow 逐字节同构，sheen_color 是打包的 unorm4x8（授权侧为 Color4f），reserved0 补齐至 64B 整倍数，必须保留。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_PBR_ADVANCED_SURFACE_ROW_GLSL
#define HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_PBR_ADVANCED_SURFACE_ROW_GLSL

struct PBRAdvancedSurfaceData
{
    vec4 base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
    float clearcoat;
    float clearcoat_roughness;
    float sheen_roughness;
    float anisotropy;
    uint sheen_color;
    float subsurface_power;
    float thickness;
    uint reserved0;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer PBRAdvancedSurfaceRow
{
    vec4 base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
    float clearcoat;
    float clearcoat_roughness;
    float sheen_roughness;
    float anisotropy;
    uint sheen_color;
    float subsurface_power;
    float thickness;
    uint reserved0;
};

#endif // HGL_SHADERLIB_MODULE_DESCRIPTOR_SSBO_PBR_ADVANCED_SURFACE_ROW_GLSL
