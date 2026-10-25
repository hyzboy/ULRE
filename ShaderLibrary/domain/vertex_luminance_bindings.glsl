// vertex_luminance_bindings.glsl — VertexLuminance（顶点亮度自发光）域的数据布局。
//
// 亮度来自顶点 varying，但表面参数仍走**材质私有数据行**：该领域契约的
// `material_private_data` 是 `GlobalSSBOType::EmissiveSurface`（不是
// StandardOpaque 的 `PBRSurface`），行跨度 16 字节，所以绑定文件不能共用。
//
// 契约里贴图槽表为空 → 没有 `MaterialTextureReferencesRef`，也没有采样器宏。
#ifndef HGL_DOMAIN_VERTEX_LUMINANCE_BINDINGS_GLSL
#define HGL_DOMAIN_VERTEX_LUMINANCE_BINDINGS_GLSL

// ── 材质私有数据行（GlobalSSBOType::EmissiveSurface） ──
struct EmissiveSurfaceData
{
    vec4 color;
};
layout(buffer_reference, scalar, buffer_reference_align=16) buffer EmissiveSurfaceRow
{
    vec4 color;
};
// 行跨度 16 字节 = 4 × 4，与 EmissiveSurfaceData 一致。
#define MTL_ROW(i) EmissiveSurfaceRow(global_addresses.addr_emissive_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(16))

#endif // HGL_DOMAIN_VERTEX_LUMINANCE_BINDINGS_GLSL
