#include <hgl/shadergen/MaterialDescriptorContract.h>

#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/util/hash/FNV1a.h>
#include <algorithm>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        uint64 HashText(const char *text) noexcept
        {
            if (!text || !text[0])
                return 0;
            return hgl::hash::FNV1aAppendBytes(
                hgl::hash::FNV1aInit<uint64>(),
                text,
                std::strlen(text));
        }

        uint64 GetLogicalResourceID(
            const FixedDescriptorEntry &entry) noexcept
        {
            const uint64 name_hash = HashText(entry.name);
            if (name_hash != 0)
                return name_hash;

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, entry.semantic_layer);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.set_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.kind);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, entry.texture_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, entry.data_slot);
            return hgl::hash::FNV1aAppendValueBytes(
                hash, entry.ssbo_type);
        }

        uint64 GetResourceSchemaID(
            const FixedDescriptorEntry &entry) noexcept
        {
            if (entry.struct_name && entry.struct_name[0])
                return HashText(entry.struct_name);
            if (entry.glsl_type && entry.glsl_type[0])
                return HashText(entry.glsl_type);

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, entry.kind);
            return hgl::hash::FNV1aAppendValueBytes(
                hash, entry.ssbo_type);
        }

        bool AppendEntry(
            const FixedDescriptorEntry &source,
            MaterialDescriptorContract &out_contract)
        {
            MaterialDescriptorContractEntry entry{};
            entry.name = source.name ? source.name : "";
            entry.struct_name =
                source.struct_name ? source.struct_name : "";
            entry.glsl_type =
                source.glsl_type ? source.glsl_type : "";
            entry.legacy_ssbo_id = source.ssbo_id;
            entry.has_explicit_policy =
                source.has_requirement_policy;

            entry.canonical.logical_resource_id =
                GetLogicalResourceID(source);
            entry.canonical.resource_schema_id =
                GetResourceSchemaID(source);
            entry.canonical.semantic = source.semantic;
            entry.canonical.semantic_layer =
                source.semantic_layer != DescriptorSemanticLayer::Unknown
                    ? source.semantic_layer
                    : GetDescriptorSemanticLayerByKind(source.kind);
            entry.canonical.set_type = source.set_type;
            entry.canonical.kind = source.kind;
            entry.canonical.texture_slot = source.texture_slot;
            entry.canonical.ssbo_type = source.ssbo_type;
            if (source.semantic
                    == DescriptorSemantic::MaterialDataSlotData
             && entry.canonical.ssbo_type == SSBOType::UserDefined)
                entry.canonical.ssbo_type = SSBOType::PBRSurface;
            else if (source.semantic
                    == DescriptorSemantic::MaterialTextureLayerTable)
                entry.canonical.ssbo_type = SSBOType::TextureLayer;
            else if (source.semantic
                    == DescriptorSemantic::MaterialDataIndexTable)
                entry.canonical.ssbo_type =
                    SSBOType::MaterialDataIndexTable;
            else if (source.semantic
                    == DescriptorSemantic::LocalToWorldIndexTable)
                entry.canonical.ssbo_type = SSBOType::TransformIndexRows;
            entry.canonical.data_slot = source.data_slot;
            entry.canonical.stage_flags = source.stage_flags;
            entry.canonical.array_count = 1;
            entry.canonical.required =
                source.has_requirement_policy
                    ? source.required
                    : IsSemanticRequired(source.semantic);
            entry.canonical.allow_fallback =
                source.has_requirement_policy
                    ? source.allow_fallback
                    : IsSemanticFallbackAllowed(source.semantic);

            if (entry.canonical.logical_resource_id == 0
             || entry.canonical.resource_schema_id == 0)
                return false;

            out_contract.entries.emplace_back(std::move(entry));
            return true;
        }
    }

    bool BuildMaterialDescriptorContract(
        const FixedDescriptorEntry *entries,
        const uint32 entry_count,
        MaterialDescriptorContract &out_contract)
    {
        out_contract = {};
        if (entry_count > 0 && !entries)
            return false;

        out_contract.entries.reserve(entry_count);
        for (uint32 i = 0; i < entry_count; ++i)
        {
            if (!AppendEntry(entries[i], out_contract))
                return false;
        }
        return ValidateMaterialDescriptorContract(out_contract);
    }

    bool EnsureMaterialDescriptorContractVaryingResources(
        const MaterialVertexVaryingConfig &varying,
        MaterialDescriptorContract &in_out_contract)
    {
        const auto has_semantic =
            [&in_out_contract](const DescriptorSemantic semantic)
        {
            for (const MaterialDescriptorContractEntry &entry :
                 in_out_contract.entries)
            {
                if (entry.canonical.semantic == semantic)
                    return true;
            }
            return false;
        };

        std::vector<FixedDescriptorEntry> generated;
        if (varying.emit_data_index_id
         && !has_semantic(DescriptorSemantic::MaterialDataIndexTable))
        {
            FixedDescriptorEntry entry{};
            entry.set_type = DescriptorSetType::Material;
            entry.kind = DescriptorKind::SSBO;
            entry.stage_flags =
                uint32(VK_SHADER_STAGE_ALL_GRAPHICS);
            entry.name = SBS_MaterialDataIndexRows.name;
            entry.struct_name = SBS_MaterialDataIndexRows.struct_name;
            entry.semantic =
                DescriptorSemantic::MaterialDataIndexTable;
            entry.semantic_layer = DescriptorSemanticLayer::SSBO;
            entry.ssbo_type = SSBOType::MaterialDataIndexTable;
            entry.has_requirement_policy = true;
            entry.required = true;
            generated.push_back(entry);
        }

        if (varying.emit_texture_layer_id
         && !varying.texture_layer_id_uses_data_index
         && !has_semantic(
                DescriptorSemantic::MaterialTextureLayerTable))
        {
            FixedDescriptorEntry entry{};
            entry.set_type = DescriptorSetType::Material;
            entry.kind = DescriptorKind::SSBO;
            entry.stage_flags =
                uint32(VK_SHADER_STAGE_ALL_GRAPHICS);
            entry.name = SBS_MaterialTextureLayerRows.name;
            entry.struct_name =
                SBS_MaterialTextureLayerRows.struct_name;
            entry.semantic =
                DescriptorSemantic::MaterialTextureLayerTable;
            entry.semantic_layer = DescriptorSemanticLayer::SSBO;
            entry.ssbo_type = SSBOType::TextureLayer;
            entry.has_requirement_policy = true;
            entry.required = true;
            generated.push_back(entry);
        }

        for (const FixedDescriptorEntry &entry : generated)
        {
            if (!AppendEntry(entry, in_out_contract))
                return false;
        }
        return ValidateMaterialDescriptorContract(in_out_contract);
    }

    bool BuildMaterialDescriptorContract(
        const std::vector<FixedDescriptorEntry> &entries,
        MaterialDescriptorContract &out_contract)
    {
        return BuildMaterialDescriptorContract(
            entries.data(),
            static_cast<uint32>(entries.size()),
            out_contract);
    }

    bool BuildEffectiveMaterialDescriptorContract(
        const MaterialDescriptorContract &base_contract,
        const std::vector<MaterialDataSlotDecl> *data_slot_decls,
        const uint32 material_ssbo_stage_bits,
        MaterialDescriptorContract &out_contract)
    {
        out_contract = base_contract;
        if (!data_slot_decls || data_slot_decls->empty())
            return ValidateMaterialDescriptorContract(out_contract);

        out_contract.entries.erase(
            std::remove_if(
                out_contract.entries.begin(),
                out_contract.entries.end(),
                [](const MaterialDescriptorContractEntry &entry)
                {
                    return entry.canonical.semantic
                        == DescriptorSemantic::MaterialDataSlotData;
                }),
            out_contract.entries.end());

        for (uint32 i = 0;
             i < static_cast<uint32>(data_slot_decls->size());
             ++i)
        {
            const MaterialDataSlotDecl &decl = (*data_slot_decls)[i];
            FixedDescriptorEntry fixed{};
            fixed.set_type = DescriptorSetType::Material;
            fixed.kind = DescriptorKind::SSBO;
            fixed.stage_flags = material_ssbo_stage_bits;
            fixed.name = decl.name.c_str();
            fixed.struct_name =
                ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            fixed.semantic =
                DescriptorSemantic::MaterialDataSlotData;
            fixed.texture_slot = TextureSlot::BaseColor;
            fixed.data_slot = i;
            fixed.ssbo_type = decl.ssbo_type;
            fixed.semantic_layer = DescriptorSemanticLayer::SSBO;
            fixed.ssbo_id = MakeRecipeSSBOId(i);
            fixed.has_requirement_policy = true;
            fixed.required = true;
            fixed.allow_fallback = false;
            if (!AppendEntry(fixed, out_contract))
                return false;
        }

        return ValidateMaterialDescriptorContract(out_contract);
    }

    bool ConvertMaterialDescriptorContractToFixed(
        const MaterialDescriptorContract &contract,
        std::vector<FixedDescriptorEntry> &out_entries)
    {
        out_entries.clear();
        if (!ValidateMaterialDescriptorContract(contract))
            return false;

        out_entries.reserve(contract.entries.size());
        for (const MaterialDescriptorContractEntry &entry :
             contract.entries)
        {
            FixedDescriptorEntry fixed{};
            fixed.set_type = entry.canonical.set_type;
            fixed.kind = entry.canonical.kind;
            fixed.stage_flags = entry.canonical.stage_flags;
            fixed.name = entry.name.empty()
                ? nullptr : entry.name.c_str();
            fixed.struct_name = entry.struct_name.empty()
                ? nullptr : entry.struct_name.c_str();
            fixed.glsl_type = entry.glsl_type.empty()
                ? nullptr : entry.glsl_type.c_str();
            fixed.semantic = entry.canonical.semantic;
            fixed.texture_slot = entry.canonical.texture_slot;
            fixed.data_slot = entry.canonical.data_slot;
            fixed.ssbo_type = entry.canonical.ssbo_type;
            fixed.semantic_layer = entry.canonical.semantic_layer;
            fixed.ssbo_id = entry.legacy_ssbo_id;
            fixed.has_requirement_policy =
                entry.has_explicit_policy;
            fixed.required = entry.canonical.required;
            fixed.allow_fallback =
                entry.canonical.allow_fallback;
            out_entries.push_back(fixed);
        }
        return true;
    }

    bool ValidateMaterialDescriptorContract(
        const MaterialDescriptorContract &contract) noexcept
    {
        if (contract.schema_version
            != MaterialDescriptorContractSchemaVersion)
            return false;

        ShaderInterfaceContract interface_contract{};
        interface_contract.descriptor_requirements.Reserve(
            static_cast<int>(contract.entries.size()));
        for (const MaterialDescriptorContractEntry &entry :
             contract.entries)
        {
            if (entry.name.empty())
                return false;
            interface_contract.descriptor_requirements.Add(
                entry.canonical);
        }
        return ValidateShaderInterfaceContract(interface_contract);
    }

    bool BuildMaterialResourceLayoutFromDescriptorContract(
        const MaterialDescriptorContract &contract,
        MaterialResourceLayout &out_layout)
    {
        out_layout = {};
        std::vector<FixedDescriptorEntry> fixed_entries;
        if (!ConvertMaterialDescriptorContractToFixed(
                contract, fixed_entries))
            return false;

        out_layout = BuildMaterialResourceLayout(
            fixed_entries.data(),
            static_cast<uint32>(fixed_entries.size()));
        if (out_layout.requirements.size() != contract.entries.size())
            return false;

        for (size_t i = 0; i < out_layout.requirements.size(); ++i)
        {
            MaterialResourceRequirement &requirement =
                out_layout.requirements[i];
            const MaterialDescriptorContractEntry &entry =
                contract.entries[i];
            requirement.owned_name = requirement.name
                ? requirement.name : entry.name;
            requirement.owned_struct_name = requirement.struct_name
                ? requirement.struct_name : entry.struct_name;
            requirement.owned_glsl_type = requirement.glsl_type
                ? requirement.glsl_type : entry.glsl_type;
            requirement.RebindOwnedPointers();
        }
        return true;
    }

    uint64 GetMaterialDescriptorContractHash(
        const MaterialDescriptorContract &contract,
        const uint64 module_manifest_hash) noexcept
    {
        if (!ValidateMaterialDescriptorContract(contract))
            return 0;

        ShaderInterfaceContract interface_contract{};
        interface_contract.descriptor_requirements.Reserve(
            static_cast<int>(contract.entries.size()));
        for (const MaterialDescriptorContractEntry &entry :
             contract.entries)
        {
            interface_contract.descriptor_requirements.Add(
                entry.canonical);
        }

        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, MaterialDescriptorContractSchemaVersion);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, module_manifest_hash);
        return hgl::hash::FNV1aAppendValueBytes(
            hash, GetShaderInterfaceContractHash(interface_contract));
    }
}
