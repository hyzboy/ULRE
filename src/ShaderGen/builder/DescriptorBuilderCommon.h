#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/DescriptorResourceCatalog.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <cstdint>
#include <vector>

namespace hgl::graph::mtl::descriptor_builder_common
{
    using namespace hgl::graph::mtl;

    // 同构 Push 的通用入表函数（T4.2）：name/struct/semantic/ssbo_type 等差异
    // 由调用方以参数给出，消除每个 Push* 重复 9 行样板。新增顶点语义时只需
    // 加一个一行包装（数据在此集中，layer 由 semantic 推导）。
    inline void PushBySpec(
        std::vector<SerializedDescriptorEntry> &v,
        const DescriptorSetType set_type,
        const char *name,
        const char *struct_name,
        const DescriptorSemantic semantic,
        const SSBOType ssbo_type,
        const uint32_t stage_flags)
    {
        v.push_back({
            set_type, stage_flags,
            name, struct_name, nullptr, semantic,
            TextureSlot::BaseColor, DefaultMaterialPrivateDataSlot, ssbo_type,
            GetDescriptorSemanticLayer(semantic)
        });
    }

inline void PushViewport(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "viewport", "ViewportInfo", DescriptorSemantic::ViewportInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushCamera(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "camera", "CameraInfo", DescriptorSemantic::CameraInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushSky(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "sky", "SkyInfo", DescriptorSemantic::SkyInfo,
               SSBOType::UserDefined, stage_flags);
}

inline void PushMaterialColorPalette(std::vector<SerializedDescriptorEntry> &v,
                                     const uint32_t stage_flags)
{
    PushBySpec(v, DescriptorSetType::Scene,
               "color_palette", "ColorPalette", DescriptorSemantic::MaterialColorPalette,
               SSBOType::UserDefined, stage_flags);
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
        if (entry.semantic_layer != DescriptorSemanticLayer::UBO
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

inline void PushLocalToWorld(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    constexpr const DescriptorResourceCatalogEntry *row =
        FindResourceCatalogEntry(DescriptorSemantic::LocalToWorld);
    static_assert(row != nullptr && row->sbs != nullptr && row->binding >= 0,
                  "LocalToWorld 目录行缺失或非固定绑定");

    PushBySpec(v, row->set_type, row->sbs->name, row->sbs->struct_name,
               row->semantic, row->ssbo_type, stage_flags);
}

inline void PushLocalToWorldIndexRows(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    constexpr const DescriptorResourceCatalogEntry *row =
        FindResourceCatalogEntry(DescriptorSemantic::LocalToWorldIndex);
    static_assert(row != nullptr && row->sbs != nullptr && row->binding >= 0,
                  "LocalToWorldIndex 目录行缺失或非固定绑定");

    PushBySpec(v, row->set_type, row->sbs->name, row->sbs->struct_name,
               row->semantic, row->ssbo_type, stage_flags);
}

inline bool MergeSSBODescriptor(std::vector<SerializedDescriptorEntry> &v,
                               const SerializedDescriptorEntry &incoming);

inline void PushMaterialPrivateDataIndexRows(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags);

inline void PushMaterialPrivateDataSlot(std::vector<SerializedDescriptorEntry> &v,
                                 const uint32_t stage_flags,
                                 const char *name,
                                 const char *struct_name,
                                 const uint32_t material_private_data_slot,
                                 const SSBOType ssbo_type)
{
    // 数据槽无描述符：需求降级为地址行表条目
    //（EnsureMaterialPrivateDataIndexTable 依赖 MaterialPrivateData 条目
    // 判定是否需要行表；此处的 name/struct 参数已无消费方，保留签名兼容）。
    PushMaterialPrivateDataIndexRows(v, stage_flags);
}

inline void PushMaterialPrivateDataIndexRows(std::vector<SerializedDescriptorEntry> &v, const uint32_t stage_flags)
{
    // PerObject 行表：8B 设备地址表（mtl_data_addrs）。
    // 经 MergeSSBODescriptor 合并同身份条目——数据槽需求可能同时来自
    // 定义 TOML 与模块 manifest（双来源），重复条目会破坏契约校验。
    SerializedDescriptorEntry entry{};
    entry.set_type                = DescriptorSetType::PerObject;
    entry.stage_flags             = stage_flags;
    entry.name                    = SBS_MaterialDataAddresses.name;
    entry.struct_name             = SBS_MaterialDataAddresses.struct_name;
    entry.semantic                = DescriptorSemantic::MaterialPrivateDataIndex;
    entry.semantic_layer          = DescriptorSemanticLayer::SSBO;
    entry.ssbo_type               = SSBOType::MaterialPrivateDataIndex;
    entry.material_private_data_slot = DefaultMaterialPrivateDataSlot;
    MergeSSBODescriptor(v, entry);
}

inline void AppendDefinitionMaterialDescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    const MaterialDefinition &definition,
    const uint32_t stage_flags)
{
    // 单槽化：固定 slot 0 / DefaultMaterialPrivateDataSlotName
    if (definition.material_private_data != SSBOType::UserDefined)
    {
        PushMaterialPrivateDataSlot(
            v,
            stage_flags,
            DefaultMaterialPrivateDataSlotName,
            ssbo::GetMaterialSSBOStructName(definition.material_private_data),
            DefaultMaterialPrivateDataSlot,
            definition.material_private_data);

        PushMaterialPrivateDataIndexRows(v, stage_flags);
    }

    // 纹理句柄已全部随数据槽行尾（tex_tail）下发——mtl_texture_layer_rows
    // 描述符与 Material 集行表已退场，不再注入任何纹理行表条目。
}

    // strcmp 包装（唯一实现，原三处副本收敛于此）：
    // 带 <0x10000 指针防御——manifest/描述符条目指针损坏时不比
    // 较、返回不等（原 MaterialShaderCompiler CStrEq 的防御语义），
    // 避免对垃圾指针解引用。
    inline bool CStrEqual(const char *lhs, const char *rhs) noexcept
    {
        if (lhs && reinterpret_cast<uintptr_t>(lhs) < 0x10000u)
            return false;
        if (rhs && reinterpret_cast<uintptr_t>(rhs) < 0x10000u)
            return false;
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
        if (existing.semantic_layer != DescriptorSemanticLayer::SSBO
         || incoming.semantic_layer != DescriptorSemanticLayer::SSBO)
            continue;

        const bool same_name = CStrEqual(existing.name, incoming.name);
        const bool same_semantic_slot =
            existing.semantic == incoming.semantic
         && existing.material_private_data_slot == incoming.material_private_data_slot;
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
    const ShaderCodeModuleSSBORequirement &ssbo)
{
    if (!ssbo.name || !*ssbo.name)
        return false;

    SerializedDescriptorEntry entry{};
    entry.stage_flags = ssbo.stage_flags;
    entry.name = ssbo.name;
    entry.struct_name = ssbo::GetMaterialSSBOStructName(ssbo.ssbo_type);
    entry.material_private_data_slot = ssbo.material_private_data_slot;
    entry.ssbo_type = ssbo.ssbo_type;
    entry.ssbo_id = MakeRecipeSSBOId(ssbo.material_private_data_slot);
    entry.has_requirement_policy = true;
    entry.required = ssbo.required;
    entry.allow_fallback = ssbo.allow_fallback;

    // 材质私有数据 SSBO：单槽方案下固定 material_private_data_slot == 0（MaterialPrivateData）。
    if (ssbo.material_private_data_slot != DefaultMaterialPrivateDataSlot)
        return false;

    // 数据槽无描述符，需求桥接为地址行表条目。
    // 非材质行类型（如 TextureLayer）无意义——显式冲突失败。
    if (!IsMaterialSSBOType(ssbo.ssbo_type))
        return false;   // 调用方置 ResourceConflict

    // 数据槽无描述符（BDA 行尾寻址）——仅需求行表条目，schema 不再含数据槽本身
    PushMaterialPrivateDataIndexRows(v, entry.stage_flags);
    return true;
}



inline bool AppendManifestSSBODescriptors(
    std::vector<SerializedDescriptorEntry> &v,
    ShaderCodeResourceManifest &manifest)
{
    for (uint32 i = 0; i < manifest.ssbo_count; ++i)
    {
        if (PushManifestSSBO(v, manifest.ssbos[i]))
            continue;

        manifest.error = ShaderCodeResourceManifestError::ResourceConflict;
        return false;
    }

    return true;
}


// Provider modules may declare their own "mtl"-style data-slot SSBO purely via
// manifest metadata (`@ulre ssbo ...`), without the material TOML also listing it
// under [resources].ssbos. In that case `definition.material_private_data_slot_decls` stays empty,
// so AppendDefinitionMaterialDescriptors() never pushes the MaterialPrivateDataIndexRows
// index table — yet ResolveMaterialPrivateDataIndex() is still referenced unconditionally by
// the vertex assembler. Scan the merged descriptor list for any
// MaterialPrivateData entry (from either source) and make sure the matching
// MaterialPrivateDataIndex entry exists.

inline bool BuildDefinitionShaderCodeResourceManifest(
    const MaterialDefinition &definition,
    ShaderCodeResourceManifest &manifest,
    const char *const *extra_roots = nullptr,
    const uint32 extra_root_count = 0,
    const ShaderCodeModuleRegistry *registry = nullptr)
{
    const char *roots[MaxShaderCodeResourceManifestCodeModules]{};
    uint32 root_count = 0;
    for (const auto &name : definition.code_module_requirements)
    {
        if (root_count >= MaxShaderCodeResourceManifestCodeModules)
            return false;
        roots[root_count++] = name.c_str();
    }

    if (extra_root_count > 0 && !extra_roots)
        return false;
    for (uint32 i = 0; i < extra_root_count; ++i)
    {
        if (root_count >= MaxShaderCodeResourceManifestCodeModules)
            return false;
        roots[root_count++] = extra_roots[i];
    }

    return BuildShaderCodeResourceManifest(roots, root_count, manifest, registry);
}

} // namespace hgl::graph::mtl::descriptor_builder_common
