// sky_bindings.glsl — Sky（天空环境光探针）域的数据布局。
//
// Sky 是唯一没有材质私有数据行的域——天空盒纯靠方向向量与一张 cube 贴图，
// 所以这里不出现 `MTL_ROW`，只有单槽贴图表。
//
// ⚠ 槽位顺序与跨度 = `DomainContracts[]` 里 Sky 的 SkySlots 表：
// 只有 sky_cube（可选），跨度 16 字节（uvec2 + 对齐）。
#ifndef HGL_DOMAIN_SKY_BINDINGS_GLSL
#define HGL_DOMAIN_SKY_BINDINGS_GLSL

// 采样器预设索引（contract/sampler.toml 全局序号）。天空盒只有 trilinear 一档。
#define TrilinearSampler 2u

layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_sky_cube;
};
#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))

#endif // HGL_DOMAIN_SKY_BINDINGS_GLSL
