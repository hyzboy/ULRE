// unlit_texture_bindings.glsl — UnlitTexture（无光单图）域的数据布局，物理固定。
//
// UnlitTexture 只有一张图，且**不读材质私有数据行**：颜色完全来自
// `material/texture_source.glsl` 对 base_color 的采样。因此这里的贴图表只有
// 一个元素（跨度 16 = uvec2 + 对齐），也没有 `MTL_ROW`。
//
// ⚠ 槽位顺序 = `DomainContracts[]` 里 UnlitTexture 的 UnlitTextureSlots
// （只有 base_color），顺序即索引。
#ifndef HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL
#define HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL

#define TrilinearSampler 2u

layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_base_color;
};
#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))

#endif // HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL
