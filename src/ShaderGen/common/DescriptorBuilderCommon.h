#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/ModuleResourceManifest.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <vector>

namespace hgl::graph::mtl::descriptor_builder_common
{
    using namespace hgl::graph::mtl;

inline void PushViewport(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushCamera(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushSky(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushMaterialColorPalette(std::vector<SerializedDescriptorEntry> &v,
                                     const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "color_palette", "ColorPalette", nullptr, DescriptorSemantic::MaterialColorPalette,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined,
        DescriptorSemanticLayer::UBO
    });
}

inline void MergeUBODescriptor(
    std::vector<SerializedDescriptorEntry> &v,
    const DescriptorSemantic semantic,
    const uint32_t stage_flags,
    const bool has_policy = false,
    const bool required = true,
    const bool allow_fallback = false)
{
    for (auto &entry : v)
    {
        if (entry.kind != DescriptorKind::UBO
         || entry.semantic != semantic)
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
    case DescriptorSemantic::ViewportInfo:
        PushViewport(v, stage_flags);
        break;
    case DescriptorSemantic::CameraInfo:
        PushCamera(v, stage_flags);
        break;
    case DescriptorSemantic::SkyInfo:
        PushSky(v, stage_flags);
        break;
    case DescriptorSemantic::MaterialColorPalette:
        PushMaterialColorPalette(v, stage_flags);
        break;
    }
    if (has_policy && !v.empty())
    {
        SerializedDescriptorEntry &entry = v.back();
        entry.has_requirement_policy = true;
        entry.required = required;
        entry.allow_fallback = allow_fallback;
    }
}

inline void AppendDefinitionUBODescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t default_stage_flags,
    const uint32_t sky_stage_flags,
    const uint32_t color_palette_stage_flags)
{
    for (const DescriptorSemantic semantic : definition.ubo_requirements)
    {
        const uint32_t stage_flags =
            semantic == DescriptorSemantic::SkyInfo
                ? sky_stage_flags
                : semantic == DescriptorSemantic::MaterialColorPalette
                    ? color_palette_stage_flags
                    : default_stage_flags;
        MergeUBODescriptor(v, semantic, stage_flags);
    }
}

inline void PushLocalToWorld(std::vector<SerializedDescriptorEntry> &v, const DescriptorKind kind, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, kind, stage_flags,
        "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(kind)
    });
}

inline void PushLocalToWorldIndexRows(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

// ── 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）──
// PerObject 集固定 binding（kPerObjectBindingVertex*），固定名路径
inline void PushVertexPosition(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        SBS_VertexPosition.name, SBS_VertexPosition.struct_name, nullptr, DescriptorSemantic::VertexPosition,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::VertexPosition, DescriptorSemanticLayer::SSBO
    });
}

inline void PushVertexUV(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        SBS_VertexUV.name, SBS_VertexUV.struct_name, nullptr, DescriptorSemantic::VertexUV,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::VertexUV, DescriptorSemanticLayer::SSBO
    });
}

inline void PushVertexNTB(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        SBS_VertexNTB.name, SBS_VertexNTB.struct_name, nullptr, DescriptorSemantic::VertexNTB,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::VertexNTB, DescriptorSemanticLayer::SSBO
    });
}

inline void PushVertexJoint(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        SBS_VertexJoint.name, SBS_VertexJoint.struct_name, nullptr, DescriptorSemantic::VertexJoint,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::VertexJoint, DescriptorSemanticLayer::SSBO
    });
}

inline void PushVertexIndex(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        SBS_VertexIndex.name, SBS_VertexIndex.struct_name, nullptr, DescriptorSemantic::VertexIndex,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::VertexIndex, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialDataSlot(std::vector<SerializedDescriptorEntry> &v,
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

inline void PushMaterialDataIndexRows(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        // P1-2c：mtl_data_index_rows 迁至 PerObject 集（实例→材质行索引表）。
        DescriptorSetType::PerObject, DescriptorKind::SSBO, stage_flags,
        "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::MaterialDataIndexTable, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialTextureLayerRows(
    std::vector<SerializedDescriptorEntry> &v,
    const uint32_t stage_flags,
    const bool has_policy = false,
    const bool required = true,
    const bool allow_fallback = false)
{
    SerializedDescriptorEntry entry{
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable,
        TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    };
    entry.has_requirement_policy = has_policy;
    entry.required = required;
    entry.allow_fallback = allow_fallback;
    v.push_back(entry);
}

inline void AppendDefinitionMaterialDescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t stage_flags,
    const uint32_t texture_layer_table_stage_flags)
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
        PushMaterialDataIndexRows(v, stage_flags);

    // P1-2e：mtl_texture_layer_rows 仅当材质声明纹理槽时才要求。
    // 有数据槽但无纹理槽的材质（PureColor 等）不再隐式要求
    // MaterialTextureLayerTable；纹理层行表改由
    // AppendManifestTextureLayerDescriptors（manifest 元数据）或此处
    // texture_slot_decls 声明提供。
    if (!definition.texture_slot_decls.empty())
        PushMaterialTextureLayerRows(v, texture_layer_table_stage_flags);
}

inline bool CStrEqual(const char *lhs, const char *rhs) noexcept
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

inline void MergeResourcePolicy(
    SerializedDescriptorEntry &existing,
    const SerializedDescriptorEntry &incoming)
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

inline bool MergeSSBODescriptor(
    std::vector<SerializedDescriptorEntry> &v,
    const SerializedDescriptorEntry &incoming)
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
    std::vector<SerializedDescriptorEntry> &v,
    const GLSLCodeModuleSSBORequirement &ssbo)
{
    if (!ssbo.name || !*ssbo.name)
        return false;

    SerializedDescriptorEntry entry{};
    entry.stage_flags = ssbo.stage_flags;
    entry.name = ssbo.name;
    entry.struct_name = ssbo::GetMaterialSSBOStructName(ssbo.ssbo_type);
    entry.data_slot = ssbo.data_slot;
    entry.ssbo_type = ssbo.ssbo_type;
    entry.ssbo_id = MakeRecipeSSBOId(ssbo.data_slot);
    entry.has_requirement_policy = true;
    entry.required = ssbo.required;
    entry.allow_fallback = ssbo.allow_fallback;

    // 顶点数据 SSBO（MeshShader 方向）：PerObject 集 + 固定 binding（kPerObjectBindingVertex*）
    switch (ssbo.ssbo_type)
    {
    case SSBOType::VertexPosition:
        entry.set_type = DescriptorSetType::PerObject;
        entry.kind = DescriptorKind::SSBO;
        entry.semantic = DescriptorSemantic::VertexPosition;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        break;
    case SSBOType::VertexUV:
        entry.set_type = DescriptorSetType::PerObject;
        entry.kind = DescriptorKind::SSBO;
        entry.semantic = DescriptorSemantic::VertexUV;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        break;
    case SSBOType::VertexNTB:
        entry.set_type = DescriptorSetType::PerObject;
        entry.kind = DescriptorKind::SSBO;
        entry.semantic = DescriptorSemantic::VertexNTB;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        break;
    case SSBOType::VertexJoint:
        entry.set_type = DescriptorSetType::PerObject;
        entry.kind = DescriptorKind::SSBO;
        entry.semantic = DescriptorSemantic::VertexJoint;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        break;
    default:
        entry.set_type = DescriptorSetType::Material;
        entry.kind = DescriptorKind::SSBO;
        entry.semantic = DescriptorSemantic::MaterialDataSlotData;
        entry.semantic_layer = DescriptorSemanticLayer::SSBO;
        break;
    }
    return MergeSSBODescriptor(v, entry);
}

inline void AppendManifestUBODescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    const ModuleResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ubo_count; ++i)
    {
        const auto &ubo = manifest.ubos[i];
        MergeUBODescriptor(v, ubo.semantic, ubo.stage_flags, true,
                           ubo.required, ubo.allow_fallback);
    }
}

inline bool AppendManifestSSBODescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    ModuleResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ssbo_count; ++i)
    {
        if (PushManifestSSBO(v, manifest.ssbos[i]))
            continue;

        manifest.error = ModuleResourceManifestError::ResourceConflict;
        return false;
    }

    return true;
}

inline bool AppendManifestTextureLayerDescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    ModuleResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.texture_layer_count; ++i)
    {
        const auto &layer = manifest.texture_layers[i];
        SerializedDescriptorEntry entry{};
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
            manifest.error = ModuleResourceManifestError::ResourceConflict;
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
    std::vector<SerializedDescriptorEntry> &v,
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

inline bool BuildDefinitionModuleResourceManifest(
    const MaterialDefinition &definition,
    ModuleResourceManifest &manifest,
    const char *const *extra_roots = nullptr,
    const uint32 extra_root_count = 0,
    const GLSLCodeModuleRegistry *registry = nullptr)
{
    const char *roots[MaxModuleResourceManifestCodeModules]{};
    uint32 root_count = 0;
    for (const auto &name : definition.code_module_requirements)
    {
        if (root_count >= MaxModuleResourceManifestCodeModules)
            return false;
        roots[root_count++] = name.c_str();
    }

    if (extra_root_count > 0 && !extra_roots)
        return false;
    for (uint32 i = 0; i < extra_root_count; ++i)
    {
        if (root_count >= MaxModuleResourceManifestCodeModules)
            return false;
        roots[root_count++] = extra_roots[i];
    }

    return BuildModuleResourceManifest(roots, root_count, manifest, registry);
}

inline uint64 HashDescriptorEntries(
    const std::vector<SerializedDescriptorEntry> &entries) noexcept
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
    const std::vector<SerializedDescriptorEntry> &entries) noexcept
{
    hgl::hash::FNV1aHasher64 h;
    constexpr uint32 contract_version = 2u;
    const ShaderResourceSchema schema =
        BuildShaderResourceSchema(entries.data(), static_cast<uint32>(entries.size()));
    h << contract_version
      << manifest_hash
      << HashShaderResourceSchema(schema);
    return h;
}

} // namespace hgl::graph::mtl::descriptor_builder_common
