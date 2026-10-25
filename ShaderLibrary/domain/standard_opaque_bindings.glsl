// standard_opaque_bindings.glsl — StandardOpaque（≈ UE 的 Material 主材质）
// 的数据布局，物理固定。
//
// 与 root_bindings.glsl 的分工：
//   root_bindings        —— 引擎级、跨 domain 唯一（pc_root / 材质实例行 / 扩展）
//   <domain>_bindings    —— 由 `DomainContracts[].material_private_data`
//                           （GlobalSSBOType）+ 贴图槽表决定的物理布局
//
// 此前这两层都由 ShaderGen 的 `BuildMaterialSSBODeclarations` /
// `BuildMaterialResourceDocument` 逐次生成；它们只依赖 domain，不依赖任何
// `.mi.toml`，所以生成是纯多余——现在直接写成物理文件。
//
// ⚠ 槽位顺序 = `DomainContracts[]` 里 StandardOpaque 的 texture_slots 顺序
// （base_color, roughness, metallic, occlusion, opacity_mask, normal）。
// 顺序即索引，改动必须同步 MaterialDomain.cpp，否则贴图整表错位。
#ifndef HGL_DOMAIN_STANDARD_OPAQUE_BINDINGS_GLSL
#define HGL_DOMAIN_STANDARD_OPAQUE_BINDINGS_GLSL

// ── 采样器预设索引 ──
// 数值是 sampler.toml 里的**全局**预设序号（SamplerPresetLibrary::GetIndex），
// 不是本 domain 内的序号。写错不会报错，会静默按 Nearest 采样。
//
// ⚠ 这三个 `#define` 原本由 `BuildSamplerMacros(definition->sampler_names)`
// 逐次生成。实测同一 domain 两次运行会产出**不同的**宏集合（第二次漏掉全部
// 采样器宏，sky 因此编不过）——生成依赖 definition 的加载顺序，属于隐患。
// 采样器槽表是 `DomainContracts[]` 里的静态数据，所以这里直接物理固定，
// 不再有任何生成环节。
#define TrilinearSampler 2u
#define LinearSampler 1u
#define ShadowPCFSampler 4u

// ── 材质私有数据行（GlobalSSBOType::PBRSurface） ──
struct PBRSurfaceData
{
    vec4  base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
};
layout(buffer_reference, scalar, buffer_reference_align=16) buffer PBRSurfaceRow
{
    vec4  base_color;
    float metallic;
    float roughness;
    float normal_scale;
    float fresnel;
};
// 行跨度 32 字节 = 8 × 4，与 PBRSurfaceData 一致。
#define MTL_ROW(i) PBRSurfaceRow(global_addresses.addr_pbr_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(32))

// ── 材质贴图表 ──
// 每个元素是一对 (bindless 索引, 采样器索引)，16 字节对齐；跨度 48 = 6 × 8。
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_base_color;
    uvec2 tex_roughness;
    uvec2 tex_metallic;
    uvec2 tex_occlusion;
    uvec2 tex_opacity_mask;
    uvec2 tex_normal;
};
#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(48))

#endif // HGL_DOMAIN_STANDARD_OPAQUE_BINDINGS_GLSL
