#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <vector>

namespace hgl::graph::shadergen::descriptor_builder_common
{
    using namespace hgl::graph::mtl;

inline const char *GetTextureNameBySlot(const TextureSlot slot) noexcept
{
    switch (slot)
    {
    case TextureSlot::BaseColor: return "TextureBaseColor";
    case TextureSlot::Normal: return "TextureNormal";
    case TextureSlot::Metallic: return "TextureMetallic";
    case TextureSlot::Roughness: return "TextureRoughness";
    case TextureSlot::Emissive: return "TextureEmissive";
    case TextureSlot::Occlusion: return "TextureOcclusion";
    case TextureSlot::OpacityMask: return "TextureOpacityMask";
    case TextureSlot::Height: return "TextureHeight";
    case TextureSlot::Custom0: return "TextureCustom0";
    case TextureSlot::Custom1: return "TextureCustom1";
    }

    return "TextureUnknown";
}

inline void PushViewport(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushCamera(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushSky(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushMaterialColorPalette(std::vector<FixedDescriptorEntry> &v,
                                     const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::UBO, stage_flags,
        "color_palette", "ColorPalette", nullptr, DescriptorSemantic::MaterialColorPalette,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined,
        DescriptorSemanticLayer::UBO
    });
}

inline void MergeUBODescriptor(
    std::vector<FixedDescriptorEntry> &v,
    const UBODescriptorSemantic semantic,
    const uint32_t stage_flags,
    const bool has_policy = false,
    const bool required = true,
    const bool allow_fallback = false)
{
    for (auto &entry : v)
    {
        UBODescriptorSemantic existing{};
        if (entry.kind != DescriptorKind::UBO
         || !TryGetUBODescriptorSemantic(entry.semantic, existing)
         || existing != semantic)
            continue;

        entry.stage_flags |= stage_flags;
        if (has_policy)
        {
            entry.has_requirement_policy = true;
            entry.required = entry.required || required;
            entry.allow_fallback = entry.allow_fallback && allow_fallback;
        }
        return;
    }

    switch (semantic)
    {
    case UBODescriptorSemantic::ViewportInfo:
        PushViewport(v, stage_flags);
        break;
    case UBODescriptorSemantic::CameraInfo:
        PushCamera(v, stage_flags);
        break;
    case UBODescriptorSemantic::SkyInfo:
        PushSky(v, stage_flags);
        break;
    case UBODescriptorSemantic::MaterialColorPalette:
        PushMaterialColorPalette(v, stage_flags);
        break;
    }
    if (has_policy && !v.empty())
    {
        FixedDescriptorEntry &entry = v.back();
        entry.has_requirement_policy = true;
        entry.required = required;
        entry.allow_fallback = allow_fallback;
    }
}

inline void AppendDefinitionUBODescriptors(
    std::vector<FixedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t default_stage_flags,
    const uint32_t sky_stage_flags,
    const uint32_t color_palette_stage_flags)
{
    for (const UBODescriptorSemantic semantic : definition.ubo_requirements)
    {
        const uint32_t stage_flags =
            semantic == UBODescriptorSemantic::SkyInfo
                ? sky_stage_flags
                : semantic == UBODescriptorSemantic::MaterialColorPalette
                    ? color_palette_stage_flags
                    : default_stage_flags;
        MergeUBODescriptor(v, semantic, stage_flags);
    }
}

inline void PushLocalToWorld(std::vector<FixedDescriptorEntry> &v, const DescriptorKind kind, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Transform, kind, stage_flags,
        "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(kind)
    });
}

inline void PushLocalToWorldIndexRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Transform, DescriptorKind::SSBO, stage_flags,
        "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialDataSlot(std::vector<FixedDescriptorEntry> &v,
                                 const uint32_t stage_flags,
                                 const char *name,
                                 const char *struct_name,
                                 const uint32_t data_slot,
                                 const SSBOType ssbo_type)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        name, struct_name, nullptr, DescriptorSemantic::MaterialDataSlotData,
        TextureSlot::BaseColor, data_slot, ssbo_type, GetDescriptorSemanticLayerByKind(DescriptorKind::SSBO)
    });
}

inline void PushMaterialDataIndexRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialTextureLayerRows(
    std::vector<FixedDescriptorEntry> &v,
    const uint32_t stage_flags,
    const bool has_policy = false,
    const bool required = true,
    const bool allow_fallback = false)
{
    FixedDescriptorEntry entry{
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    };
    entry.has_requirement_policy = has_policy;
    entry.required = required;
    entry.allow_fallback = allow_fallback;
    v.push_back(entry);
}

inline void PushMaterialSampler(std::vector<FixedDescriptorEntry> &v,
                                const char *name,
                                const TextureSlot slot,
                                const char *glsl_type,
                                const uint32_t stage_flags,
                                const bool required)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::TextureSampler, stage_flags,
        name, nullptr, glsl_type, DescriptorSemantic::MaterialSampler,
        slot, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler,
        MakeRecipeSSBOId(0), true, required, !required
    });
}

inline void AppendDefinitionMaterialDescriptors(
    std::vector<FixedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t stage_flags,
    const uint32_t texture_layer_table_stage_flags,
    const uint32_t texture_stage_flags)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(definition.data_slot_decls.size()); ++i)
    {
        const auto &decl = definition.data_slot_decls[i];
        PushMaterialDataSlot(
            v,
            stage_flags,
            decl.name.c_str(),
            ssbo::GetMaterialSSBOStructName(decl.ssbo_type),
            i,
            decl.ssbo_type);
    }

    if (!definition.data_slot_decls.empty())
    {
        PushMaterialDataIndexRows(v, stage_flags);
        PushMaterialTextureLayerRows(v, texture_layer_table_stage_flags);
    }

    for (const auto &decl : definition.texture_slot_decls)
    {
        const char *name = decl.name ? decl.name : GetTextureNameBySlot(decl.slot);
        PushMaterialSampler(
            v,
            name,
            decl.slot,
            ToGLSLSamplerTypeName(decl.sampler_type),
            texture_stage_flags,
            decl.required);
    }
}

inline bool IsTextureDescriptor(const FixedDescriptorEntry &entry) noexcept
{
    return entry.kind == DescriptorKind::Texture
        || entry.kind == DescriptorKind::TextureSampler;
}

inline bool CStrEqual(const char *lhs, const char *rhs) noexcept
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

inline void MergeResourcePolicy(
    FixedDescriptorEntry &existing,
    const FixedDescriptorEntry &incoming)
{
    const bool existing_required = existing.has_requirement_policy
        ? existing.required : IsSemanticRequired(existing.semantic);
    const bool existing_fallback = existing.has_requirement_policy
        ? existing.allow_fallback : IsSemanticFallbackAllowed(existing.semantic);
    const bool incoming_required = incoming.has_requirement_policy
        ? incoming.required : IsSemanticRequired(incoming.semantic);
    const bool incoming_fallback = incoming.has_requirement_policy
        ? incoming.allow_fallback : IsSemanticFallbackAllowed(incoming.semantic);

    existing.has_requirement_policy = true;
    existing.required = existing_required || incoming_required;
    existing.allow_fallback = existing_fallback && incoming_fallback;
}

inline bool MergeTextureDescriptor(
    std::vector<FixedDescriptorEntry> &v,
    const FixedDescriptorEntry &incoming)
{
    for (auto &existing : v)
    {
        if (!IsTextureDescriptor(existing))
            continue;

        const bool same_name = CStrEqual(existing.name, incoming.name);
        const bool same_semantic_slot =
            existing.semantic == incoming.semantic
         && existing.texture_slot == incoming.texture_slot;
        if (!same_name && !same_semantic_slot)
            continue;

        const bool same_identity =
            same_name
         && same_semantic_slot
         && existing.set_type == incoming.set_type
         && existing.kind == incoming.kind
         && existing.semantic_layer == incoming.semantic_layer
         && ((existing.glsl_type == nullptr && incoming.glsl_type == nullptr)
          || CStrEqual(existing.glsl_type, incoming.glsl_type));
        if (!same_identity)
            return false;

        existing.stage_flags |= incoming.stage_flags;
        MergeResourcePolicy(existing, incoming);
        return true;
    }

    v.push_back(incoming);
    return true;
}

inline bool MergeSSBODescriptor(
    std::vector<FixedDescriptorEntry> &v,
    const FixedDescriptorEntry &incoming)
{
    for (auto &existing : v)
    {
        if (existing.kind != DescriptorKind::SSBO
         || incoming.kind != DescriptorKind::SSBO)
            continue;

        const bool same_name = CStrEqual(existing.name, incoming.name);
        const bool same_semantic_slot =
            existing.semantic == incoming.semantic
         && existing.data_slot == incoming.data_slot;
        if (!same_name && !same_semantic_slot)
            continue;

        const bool same_identity =
            same_name
         && same_semantic_slot
         && existing.set_type == incoming.set_type
         && existing.ssbo_type == incoming.ssbo_type
         && existing.ssbo_id == incoming.ssbo_id
         && existing.semantic_layer == incoming.semantic_layer;
        if (!same_identity)
            return false;

        existing.stage_flags |= incoming.stage_flags;
        MergeResourcePolicy(existing, incoming);
        return true;
    }

    v.push_back(incoming);
    return true;
}

inline bool PushManifestSSBO(
    std::vector<FixedDescriptorEntry> &v,
    const GLSLCodeModuleSSBORequirement &ssbo)
{
    if (!ssbo.name || !*ssbo.name)
        return false;

    FixedDescriptorEntry entry{};
    entry.set_type = DescriptorSetType::Material;
    entry.kind = DescriptorKind::SSBO;
    entry.stage_flags = ssbo.stage_flags;
    entry.name = ssbo.name;
    entry.struct_name = ssbo::GetMaterialSSBOStructName(ssbo.ssbo_type);
    entry.semantic = DescriptorSemantic::MaterialDataSlotData;
    entry.data_slot = ssbo.data_slot;
    entry.ssbo_type = ssbo.ssbo_type;
    entry.ssbo_id = MakeRecipeSSBOId(ssbo.data_slot);
    entry.semantic_layer = DescriptorSemanticLayer::SSBO;
    entry.has_requirement_policy = true;
    entry.required = ssbo.required;
    entry.allow_fallback = ssbo.allow_fallback;
    return MergeSSBODescriptor(v, entry);
}

inline bool MakeManifestTextureDescriptor(
    const GLSLCodeModuleTextureRequirement &texture,
    FixedDescriptorEntry &out)
{
    if (!texture.name || !*texture.name
     || !texture.glsl_type || !*texture.glsl_type)
        return false;

    out = {};
    out.stage_flags = texture.stage_flags;
    out.name = texture.name;
    out.glsl_type = texture.glsl_type;
    out.texture_slot = texture.slot;
    out.semantic = texture.semantic;
    out.has_requirement_policy = true;
    out.required = texture.required;
    out.allow_fallback = texture.allow_fallback;

    switch (texture.semantic)
    {
    case DescriptorSemantic::SkyCubemapSampler:
        out.set_type = DescriptorSetType::Scene;
        out.kind = DescriptorKind::TextureSampler;
        out.semantic_layer = DescriptorSemanticLayer::Sampler;
        break;
    case DescriptorSemantic::MaterialTexture:
        out.set_type = DescriptorSetType::Material;
        out.kind = DescriptorKind::Texture;
        out.semantic_layer = DescriptorSemanticLayer::Texture;
        break;
    case DescriptorSemantic::MaterialSampler:
        out.set_type = DescriptorSetType::Material;
        out.kind = DescriptorKind::TextureSampler;
        out.semantic_layer = DescriptorSemanticLayer::Sampler;
        break;
    default:
        return false;
    }

    return true;
}

inline void AppendManifestUBODescriptors(
    std::vector<FixedDescriptorEntry> &v,
    const ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ubo_count; ++i)
    {
        const auto &ubo = manifest.ubos[i];
        MergeUBODescriptor(v, ubo.semantic, ubo.stage_flags, true,
                           ubo.required, ubo.allow_fallback);
    }
}

inline bool AppendManifestSSBODescriptors(
    std::vector<FixedDescriptorEntry> &v,
    ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ssbo_count; ++i)
    {
        if (PushManifestSSBO(v, manifest.ssbos[i]))
            continue;

        manifest.error = ShaderResourceManifestError::ResourceConflict;
        return false;
    }

    return true;
}

inline bool AppendManifestTextureDescriptors(
    std::vector<FixedDescriptorEntry> &v,
    ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.texture_count; ++i)
    {
        FixedDescriptorEntry entry{};
        if (!MakeManifestTextureDescriptor(manifest.textures[i], entry)
         || !MergeTextureDescriptor(v, entry))
        {
            manifest.error = ShaderResourceManifestError::ResourceConflict;
            return false;
        }
    }

    return true;
}

inline bool AppendManifestTextureLayerDescriptors(
    std::vector<FixedDescriptorEntry> &v,
    ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.texture_layer_count; ++i)
    {
        const auto &layer = manifest.texture_layers[i];
        FixedDescriptorEntry entry{};
        entry.set_type = DescriptorSetType::Material;
        entry.kind = DescriptorKind::SSBO;
        entry.stage_flags = layer.stage_flags;
        entry.name = "mtl_texture_layer_rows";
        entry.struct_name = "TextureLayerRows";
        entry.semantic = DescriptorSemantic::MaterialTextureLayerTable;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        entry.has_requirement_policy = true;
        entry.required = layer.required;
        entry.allow_fallback = layer.allow_fallback;
        if (!MergeSSBODescriptor(v, entry))
        {
            manifest.error = ShaderResourceManifestError::ResourceConflict;
            return false;
        }
    }

    return true;
}

// Provider modules may declare their own "mtl"-style data-slot SSBO purely via
// manifest metadata (`@ulre ssbo ...`), without the material TOML also listing it
// under [resources].ssbos. In that case `definition.data_slot_decls` stays empty,
// so AppendDefinitionMaterialDescriptors() never pushes the mtl_data_index_rows
// index table — yet ResolveDataIndexID() is still referenced unconditionally by
// the vertex assembler. Scan the merged descriptor list for any
// MaterialDataSlotData entry (from either source) and make sure the matching
// MaterialDataIndexTable entry exists.
inline void EnsureMaterialDataIndexTable(
    std::vector<FixedDescriptorEntry> &v,
    const uint32_t stage_flags)
{
    bool has_data_slot = false;
    bool has_index_table = false;
    for (const auto &entry : v)
    {
        if (entry.semantic == DescriptorSemantic::MaterialDataSlotData)
            has_data_slot = true;
        else if (entry.semantic == DescriptorSemantic::MaterialDataIndexTable)
            has_index_table = true;
    }

    if (has_data_slot && !has_index_table)
        PushMaterialDataIndexRows(v, stage_flags);
}

inline bool BuildDefinitionShaderResourceManifest(
    const MaterialDefinition &definition,
    ShaderResourceManifest &manifest,
    const GLSLCodeModuleID *extra_roots = nullptr,
    const uint32 extra_root_count = 0,
    const GLSLCodeModuleRegistry *registry = nullptr)
{
    GLSLCodeModuleID roots[MaxShaderResourceManifestCodeModules]{};
    uint32 root_count = 0;
    for (const GLSLCodeModuleID id : definition.code_module_requirements)
    {
        if (root_count >= MaxShaderResourceManifestCodeModules)
            return false;
        roots[root_count++] = id;
    }

    if (extra_root_count > 0 && !extra_roots)
        return false;
    for (uint32 i = 0; i < extra_root_count; ++i)
    {
        if (root_count >= MaxShaderResourceManifestCodeModules)
            return false;
        roots[root_count++] = extra_roots[i];
    }

    return BuildShaderResourceManifest(roots, root_count, manifest, registry);
}

inline uint64 HashDescriptorEntries(
    const std::vector<FixedDescriptorEntry> &entries) noexcept
{
    hgl::hash::FNV1aHasher64 h;
    h << static_cast<uint32>(entries.size());

    for (const auto &entry : entries)
    {
        h << entry.set_type
          << entry.kind
          << entry.stage_flags
          << entry.semantic
          << entry.texture_slot
          << entry.data_slot
          << entry.ssbo_type
          << entry.semantic_layer
          << entry.ssbo_id
          << entry.has_requirement_policy
          << entry.required
          << entry.allow_fallback
          << entry.name
          << entry.struct_name
          << entry.glsl_type;
    }

    return h;
}

inline uint64 HashResourceContract(
    const uint64 manifest_hash,
    const std::vector<FixedDescriptorEntry> &entries) noexcept
{
    hgl::hash::FNV1aHasher64 h;
    constexpr uint32 contract_version = 2u;
    const ShaderResourceSchema layout =
        BuildMaterialResourceLayout(entries.data(), static_cast<uint32>(entries.size()));
    h << contract_version
      << manifest_hash
      << HashMaterialResourceLayout(layout);
    return h;
}

inline void ApplyMaterialDefinitionTexturePolicy(
    const MaterialDefinition &definition,
    ShaderResourceSchema &layout)
{
    for (auto &requirement : layout.resources)
    {
        if (requirement.semantic != DescriptorSemantic::MaterialTexture
         && requirement.semantic != DescriptorSemantic::MaterialSampler)
            continue;

        for (const auto &decl : definition.texture_slot_decls)
        {
            if (decl.slot != requirement.texture_slot)
                continue;

            requirement.required = decl.required;
            requirement.allow_fallback = !decl.required;
            break;
        }
    }
}

inline uint64 HashMaterialDefinitionTexturePolicy(
    const uint64 hash,
    const MaterialDefinition &definition) noexcept
{
    hgl::hash::FNV1aHasher64 h(hash);
    h << static_cast<uint32>(definition.texture_slot_decls.size());
    for (const auto &decl : definition.texture_slot_decls)
    {
        h << decl.slot
          << decl.sampler_type
          << decl.required
          << !decl.required
          << decl.name;
    }
    return h;
}

inline uint64 HashResourceContract(
    const uint64 manifest_hash,
    const std::vector<FixedDescriptorEntry> &entries,
    const MaterialDefinition &definition) noexcept
{
    hgl::hash::FNV1aHasher64 h;
    constexpr uint32 contract_version = 3u;

    ShaderResourceSchema layout =
        BuildMaterialResourceLayout(entries.data(), static_cast<uint32>(entries.size()));
    ApplyMaterialDefinitionTexturePolicy(definition, layout);
    h << contract_version
      << manifest_hash
      << HashMaterialResourceLayout(layout);
    return h;
}

} // namespace hgl::graph::shadergen::descriptor_builder_common
