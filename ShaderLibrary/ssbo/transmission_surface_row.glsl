// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MaterialDataRows.h → TransmissionSurfaceRow
//   布局：kind=ssbo bda=1 access=readwrite size=16 align=4 ref_align=16
//   说明：GlobalSSBOType::TransmissionSurface 的内容行；reserved0[3] 是补齐至 16B 整倍数的显式填充，必须保留（scalar layout 不隐式补）。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_SSBO_TRANSMISSION_SURFACE_ROW_GLSL
#define HGL_SHADERLIB_SSBO_TRANSMISSION_SURFACE_ROW_GLSL

struct TransmissionSurfaceData
{
    uint trans_color;
    uint reserved0[3];
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer TransmissionSurfaceRow
{
    uint trans_color;
    uint reserved0[3];
};

#endif // HGL_SHADERLIB_SSBO_TRANSMISSION_SURFACE_ROW_GLSL
