// ============================================================================
// standard_opaque_texture_bindings.glsl —— StandardOpaque 纹理槽引用布局（自动生成代码）
//
// 唯一真源：ShaderLibrary/contract/material_domain_slots.toml
// 由 src/Tools/DomainSlotGen/domain_slot_gen.py 自动生成，请勿手动修改！
// ============================================================================
#ifndef HGL_DOMAIN_STANDARD_OPAQUE_TEXTURE_BINDINGS_GLSL
#define HGL_DOMAIN_STANDARD_OPAQUE_TEXTURE_BINDINGS_GLSL

#define HGL_STANDARD_OPAQUE_TEXTURE_SLOT_COUNT 8
#define HGL_STANDARD_OPAQUE_TEXTURE_ROW_STRIDE 64

// ── 材质贴图表（Buffer Reference） ──
// 槽数: 8, 行跨度: 64 字节 (16字节对齐)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_base_color;
    uvec2 tex_roughness;
    uvec2 tex_metallic;
    uvec2 tex_occlusion;
    uvec2 tex_opacity_mask;
    uvec2 tex_normal;
    uvec2 tex_emissive;
    uvec2 tex_orm;
};

#define MTL_TEX_STANDARD_OPAQUE(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(64))

#ifndef MTL_TEX
#define MTL_TEX(i) MTL_TEX_STANDARD_OPAQUE(i)
#endif

#endif // HGL_DOMAIN_STANDARD_OPAQUE_TEXTURE_BINDINGS_GLSL
