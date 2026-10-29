// emissive_row_bindings.glsl — 「材质私有数据行 = GlobalSSBOType::EmissiveSurface」
// 这一族的**共享**数据布局，物理固定。
//
// 共享而不是每领域一份：这一层的全部内容只由 `DomainContracts[].material_private_data`
// 决定，与领域语义无关。四个领域的契约都选了 EmissiveSurface（只有 `vec4 color`
// 一个字段，行跨度 16 字节），因此它们的行声明逐字节相同；照领域复制四份只会
// 制造「改一处漏三处」的机会。
//
// 使用本文件的领域：UnlitSolid / VertexLuminance / ErrorChecker3D / DebugNormal。
// 契约选的是 `PBRSurface`（行跨度 32）的领域用 `standard_opaque_bindings.glsl`。
//
// 契约里贴图槽表为空的领域（本族全部）没有 `MaterialTextureReferencesRef`，
// 也没有采样器宏 —— 这里刻意不声明，缺声明会在编译期立刻暴露。
#ifndef HGL_DOMAIN_EMISSIVE_ROW_BINDINGS_GLSL
#define HGL_DOMAIN_EMISSIVE_ROW_BINDINGS_GLSL

#include "ssbo/emissive_surface_row.glsl"

// ── 材质私有数据行（GlobalSSBOType::EmissiveSurface） ──
// 结构本身见 ssbo/emissive_surface_row.glsl（由 C++ 结构 + 注解生成），
// 本文件只保留**寻址**：这一行从哪里取。
// 行跨度 16 字节 = 4 × 4，与 EmissiveSurfaceData 一致。
#define MTL_ROW(i) EmissiveSurfaceRow(global_addresses.addr_emissive_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(16))

#endif // HGL_DOMAIN_EMISSIVE_ROW_BINDINGS_GLSL
