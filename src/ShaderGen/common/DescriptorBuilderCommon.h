#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <vector>

namespace hgl::graph::mtl::descriptor_builder_common
{

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
    const uint32_t stage_flags)
{
    for (auto &entry : v)
    {
        UBODescriptorSemantic existing{};
        if (entry.kind != DescriptorKind::UBO
         || !TryGetUBODescriptorSemantic(entry.semantic, existing)
         || existing != semantic)
            continue;

        entry.stage_flags |= stage_flags;
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

inline void PushMaterialTextureLayerRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialSampler(std::vector<FixedDescriptorEntry> &v,
                                const char *name,
                                const TextureSlot slot,
                                const char *glsl_type,
                                const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::TextureSampler, stage_flags,
        name, nullptr, glsl_type, DescriptorSemantic::MaterialSampler,
        slot, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler,
        MakeRecipeSSBOId(0)
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
            texture_stage_flags);
    }
}

inline void PushManifestTexture(std::vector<FixedDescriptorEntry> &v,
                                const GLSLCodeModuleTextureRequirement &texture)
{
    const bool is_scene_sampler = texture.semantic == DescriptorSemantic::SkyCubemapSampler
                               || texture.semantic == DescriptorSemantic::MaterialSampler;
    const bool is_material_sampler = texture.semantic == DescriptorSemantic::MaterialTexture
                                 || texture.semantic == DescriptorSemantic::MaterialSampler;
    v.push_back({
        is_scene_sampler ? DescriptorSetType::Scene : DescriptorSetType::Material,
        is_scene_sampler || is_material_sampler ? DescriptorKind::TextureSampler : DescriptorKind::Texture,
        texture.stage_flags,
        texture.name,
        nullptr,
        texture.glsl_type,
        is_material_sampler ? DescriptorSemantic::MaterialSampler : texture.semantic,
        texture.slot,
        DefaultMaterialDataSlot,
        SSBOType::UserDefined,
        is_scene_sampler || is_material_sampler ? DescriptorSemanticLayer::Sampler : DescriptorSemanticLayer::Texture
    });
}

inline void AppendManifestUBODescriptors(
    std::vector<FixedDescriptorEntry> &v,
    const ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ubo_count; ++i)
    {
        const auto &ubo = manifest.ubos[i];
        MergeUBODescriptor(v, ubo.semantic, ubo.stage_flags);
    }
}

inline void AppendManifestTextureDescriptors(
    std::vector<FixedDescriptorEntry> &v,
    const ShaderResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.texture_count; ++i)
    {
        const auto &texture = manifest.textures[i];
        bool exists = false;
        for (const auto &entry : v)
        {
            if (entry.name && texture.name
             && std::strcmp(entry.name, texture.name) == 0)
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            PushManifestTexture(v, texture);
    }
}

inline bool BuildDefinitionShaderResourceManifest(
    const MaterialDefinition &definition,
    ShaderResourceManifest &manifest,
    const GLSLCodeModuleID *extra_roots = nullptr,
    const uint32 extra_root_count = 0)
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

    return BuildShaderResourceManifest(roots, root_count, manifest);
}

inline uint64 HashDescriptorEntries(
    const std::vector<FixedDescriptorEntry> &entries) noexcept
{
    uint64 hash = hgl::hash::FNV1aInit<uint64>();
    hash = hgl::hash::FNV1aAppendValueBytes(
        hash, static_cast<uint32>(entries.size()));

    const auto append_string = [](uint64 current, const char *value) -> uint64
    {
        const uint32 length = value ? static_cast<uint32>(std::strlen(value)) : 0u;
        current = hgl::hash::FNV1aAppendValueBytes(current, length);
        if (length > 0)
            current = hgl::hash::FNV1aAppendBytes(current, value, length);
        return current;
    };

    for (const auto &entry : entries)
    {
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.set_type);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.kind);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.stage_flags);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.semantic);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.texture_slot);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.data_slot);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.ssbo_type);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.semantic_layer);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.ssbo_id);
        hash = append_string(hash, entry.name);
        hash = append_string(hash, entry.struct_name);
        hash = append_string(hash, entry.glsl_type);
    }

    return hash;
}

inline uint64 HashResourceContract(
    const uint64 manifest_hash,
    const std::vector<FixedDescriptorEntry> &entries) noexcept
{
    uint64 hash = hgl::hash::FNV1aInit<uint64>();
    hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest_hash);
    hash = hgl::hash::FNV1aAppendValueBytes(hash, HashDescriptorEntries(entries));
    return hash;
}

inline uint64 HashMaterialDefinitionTexturePolicy(
    uint64 hash,
    const MaterialDefinition &definition) noexcept
{
    hash = hgl::hash::FNV1aAppendValueBytes(
        hash, static_cast<uint32>(definition.texture_slot_decls.size()));
    for (const auto &decl : definition.texture_slot_decls)
    {
        hash = hgl::hash::FNV1aAppendValueBytes(hash, decl.slot);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, decl.sampler_type);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, decl.required);
        const uint32 name_length =
            decl.name ? static_cast<uint32>(std::strlen(decl.name)) : 0u;
        hash = hgl::hash::FNV1aAppendValueBytes(hash, name_length);
        if (name_length > 0)
            hash = hgl::hash::FNV1aAppendBytes(hash, decl.name, name_length);
    }
    return hash;
}

inline uint64 HashResourceContract(
    const uint64 manifest_hash,
    const std::vector<FixedDescriptorEntry> &entries,
    const MaterialDefinition &definition) noexcept
{
    return HashMaterialDefinitionTexturePolicy(
        HashResourceContract(manifest_hash, entries), definition);
}

} // namespace hgl::graph::mtl::descriptor_builder_common
