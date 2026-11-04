// standard_advanced_bindings.glsl — StandardAdvanced（Filament 高级 PBR 域）
// 的数据布局，物理固定。与 standard_opaque_bindings.glsl 的唯一差异是
// payload 行：PBRAdvancedSurfaceRow（64B）替代 PBRSurfaceRow（32B）。
//
// 纹理槽完全复用 StandardOpaque 的规范表（material_domain_slots.toml 的
// ref_slots 别名），因此这里直接 include 它的生成物——MTL_TEX 宏在生成文件
// 里有 #ifndef 守卫，两处定义不可能分叉。
//
// ⚠ 行跨度 64 字节 = PBRAdvancedSurface 内容行（BufferGen 生成），
// 改行布局必须重跑 buffer_gen.py 并同步 GlobalSSBOType 侧，不要手改本宏。
#ifndef HGL_DOMAIN_STANDARD_ADVANCED_BINDINGS_GLSL
#define HGL_DOMAIN_STANDARD_ADVANCED_BINDINGS_GLSL

// ── 采样器预设索引 ──
// 数值是 contract/sampler.toml 里的**全局**预设序号（SamplerPresetLibrary::GetIndex），
// 一律取自生成物 contract/sampler_presets.glsl（真源 inc/hgl/shader/contract/SamplerPresetId.h）。
#include "contract/sampler_presets.glsl"

// ── 材质私有数据行（GlobalSSBOType::PBRAdvancedSurface） ──
// 结构本身见 module/descriptor/ssbo/pbr_advanced_surface_row.glsl（由 C++ 结构 + 注解生成）。
#include "module/descriptor/ssbo/pbr_advanced_surface_row.glsl"
#define MTL_ROW(i) PBRAdvancedSurfaceRow(global_addresses.addr_pbr_advanced_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(64))

// ── 材质贴图表（复用 StandardOpaque 生成物；槽序即索引） ──
#include "domain/generated/standard_opaque_texture_bindings.glsl"

#endif // HGL_DOMAIN_STANDARD_ADVANCED_BINDINGS_GLSL
