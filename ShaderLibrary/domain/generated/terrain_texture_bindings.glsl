// ============================================================================
// terrain_texture_bindings.glsl —— Terrain 纹理槽引用布局（自动生成代码）
//
// 唯一真源：ShaderLibrary/contract/material_domain_slots.toml
// 由 src/Tools/DomainSlotGen/domain_slot_gen.py 自动生成，请勿手动修改！
// ============================================================================
#ifndef HGL_DOMAIN_TERRAIN_TEXTURE_BINDINGS_GLSL
#define HGL_DOMAIN_TERRAIN_TEXTURE_BINDINGS_GLSL

#define HGL_TERRAIN_TEXTURE_SLOT_COUNT 3
#define HGL_TERRAIN_TEXTURE_ROW_STRIDE 32

// ── 材质贴图表（Buffer Reference） ──
// 槽数: 3, 行跨度: 32 字节 (16字节对齐)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_splat_map;
    uvec2 tex_base_color;
    uvec2 tex_normal;
};

#define MTL_TEX_TERRAIN(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(32))

#ifndef MTL_TEX
#define MTL_TEX(i) MTL_TEX_TERRAIN(i)
#endif

#endif // HGL_DOMAIN_TERRAIN_TEXTURE_BINDINGS_GLSL
