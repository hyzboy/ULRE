#include <hgl/shadergen/DescriptorContract.h>

#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/util/hash/FNV1a.h>
#include <algorithm>
#include <cstring>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    namespace
    {
        uint64 HashText(const char *text) noexcept
        {
            if (!text || !text[0])
                return 0;

            hgl::hash::FNV1aHasher64 h;
            h << text;
            return h;
        }

        uint64 GetLogicalResourceID(
            const SerializedDescriptorEntry &entry) noexcept
        {
            const uint64 name_hash = HashText(entry.name);
            if (name_hash != 0)
                return name_hash;

            hgl::hash::FNV1aHasher64 h;
            h << entry.semantic
              << entry.semantic_layer
              << entry.set_type
              << entry.kind
              << entry.texture_slot
              << entry.data_slot
              << entry.ssbo_type;
            return h;
        }

        uint64 GetResourceSchemaID(
            const SerializedDescriptorEntry &entry) noexcept
        {
            if (entry.struct_name && entry.struct_name[0])
                return HashText(entry.struct_name);
            if (entry.glsl_type && entry.glsl_type[0])
                return HashText(entry.glsl_type);

            hgl::hash::FNV1aHasher64 h;
            h << entry.kind
              << entry.ssbo_type;
            return h;
        }

        bool AppendEntry(
            const SerializedDescriptorEntry &source,
            DescriptorContract &out_contract)
        {
            DescriptorContractEntry entry{};
            entry.name = source.name ? source.name : "";
            entry.struct_name =
                source.struct_name ? source.struct_name : "";
            entry.glsl_type =
                source.glsl_type ? source.glsl_type : "";
            entry.ssbo_id = source.ssbo_id;
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

    bool BuildDescriptorContract(
        const SerializedDescriptorEntry *entries,
        const uint32 entry_count,
        DescriptorContract &out_contract)
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
        return ValidateDescriptorContract(out_contract);
    }

    bool EnsureDescriptorContractVaryingResources(
        const MaterialVertexVaryingConfig &varying,
        DescriptorContract &in_out_contract)
    {
        const auto has_semantic =
            [&in_out_contract](const DescriptorSemantic semantic)
        {
            for (const DescriptorContractEntry &entry :
                 in_out_contract.entries)
            {
                if (entry.canonical.semantic == semantic)
                    return true;
            }
            return false;
        };

        std::vector<SerializedDescriptorEntry> generated;
        if (varying.emit_data_index_id
         && !has_semantic(DescriptorSemantic::MaterialDataIndexTable))
        {
            SerializedDescriptorEntry entry{};
            entry.set_type = SBS_MaterialDataIndexRows.set_type;  // P1-2c：Transform 集
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

        for (const SerializedDescriptorEntry &entry : generated)
        {
            if (!AppendEntry(entry, in_out_contract))
                return false;
        }
        return ValidateDescriptorContract(in_out_contract);
    }

    bool BuildDescriptorContract(
        const std::vector<SerializedDescriptorEntry> &entries,
        DescriptorContract &out_contract)
    {
        return BuildDescriptorContract(
            entries.data(),
            static_cast<uint32>(entries.size()),
            out_contract);
    }

    bool BuildEffectiveDescriptorContract(
        const DescriptorContract &base_contract,
        const std::vector<DataSlotDeclaration> *data_slot_decls,
        const uint32 material_ssbo_stage_bits,
        DescriptorContract &out_contract)
    {
        out_contract = base_contract;
        if (!data_slot_decls || data_slot_decls->empty())
            return ValidateDescriptorContract(out_contract);

        out_contract.entries.erase(
            std::remove_if(
                out_contract.entries.begin(),
                out_contract.entries.end(),
                [](const DescriptorContractEntry &entry)
                {
                    return entry.canonical.semantic
                        == DescriptorSemantic::MaterialDataSlotData;
                }),
            out_contract.entries.end());

        for (uint32 i = 0;
             i < static_cast<uint32>(data_slot_decls->size());
             ++i)
        {
            const DataSlotDeclaration &decl = (*data_slot_decls)[i];
            SerializedDescriptorEntry fixed{};
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

        return ValidateDescriptorContract(out_contract);
    }

    bool ConvertDescriptorContractToFixed(
        const DescriptorContract &contract,
        std::vector<SerializedDescriptorEntry> &out_entries)
    {
        out_entries.clear();
        if (!ValidateDescriptorContract(contract))
            return false;

        out_entries.reserve(contract.entries.size());
        for (const DescriptorContractEntry &entry :
             contract.entries)
        {
            SerializedDescriptorEntry fixed{};
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
            fixed.ssbo_id = entry.ssbo_id;
            fixed.has_requirement_policy =
                entry.has_explicit_policy;
            fixed.required = entry.canonical.required;
            fixed.allow_fallback =
                entry.canonical.allow_fallback;
            out_entries.push_back(fixed);
        }
        return true;
    }

    bool ValidateDescriptorContract(
        const DescriptorContract &contract) noexcept
    {
        ShaderInterfaceContract interface_contract{};
        interface_contract.descriptor_requirements.Reserve(
            static_cast<int>(contract.entries.size()));
        for (const DescriptorContractEntry &entry :
             contract.entries)
        {
            if (entry.name.empty())
                return false;
            interface_contract.descriptor_requirements.Add(
                entry.canonical);
        }
        return ValidateShaderInterfaceContract(interface_contract);
    }

    namespace
    {
        DescriptorSemanticLayer NormalizeContractSemanticLayer(
            const ShaderDescriptorContractEntry &can)
        {
            if (can.semantic_layer != DescriptorSemanticLayer::Unknown)
                return can.semantic_layer;

            const DescriptorSemanticLayer mapped =
                GetDescriptorSemanticLayer(can.semantic);
            if (mapped != DescriptorSemanticLayer::Unknown)
                return mapped;

            if (can.semantic == DescriptorSemantic::LocalToWorld
             || can.semantic == DescriptorSemantic::MaterialDataSlotData)
            {
                switch (can.kind)
                {
                case DescriptorKind::UBO:
                    return DescriptorSemanticLayer::UBO;
                case DescriptorKind::SSBO:
                    return DescriptorSemanticLayer::SSBO;
                default:
                    return DescriptorSemanticLayer::Unknown;
                }
            }
            return DescriptorSemanticLayer::Unknown;
        }
    }

    bool BuildResourceSchemaFromContract(
        const DescriptorContract &contract,
        ShaderResourceSchema &out_schema)
    {
        out_schema = {};
        if (!ValidateDescriptorContract(contract))
            return false;

        out_schema.resources.reserve(contract.entries.size());

        for (const DescriptorContractEntry &entry :
             contract.entries)
        {
            const ShaderDescriptorContractEntry &can =
                entry.canonical;
            ShaderResourceSlot req;

            // ── Identity from canonical (already computed by AppendEntry) ──
            req.logical_resource_id = can.logical_resource_id;
            req.resource_schema_id = can.resource_schema_id;
            req.semantic = can.semantic;
            req.semantic_layer = NormalizeContractSemanticLayer(can);
            req.set_type = can.set_type;
            req.kind = can.kind;
            req.texture_slot = can.texture_slot;
            req.data_slot = can.data_slot;
            req.ssbo_type = can.ssbo_type;
            req.ssbo_id = entry.ssbo_id;
            req.stage_flags = can.stage_flags;
            req.required = entry.has_explicit_policy
                ? can.required
                : IsSemanticRequired(req.semantic);
            req.allow_fallback = entry.has_explicit_policy
                ? can.allow_fallback
                : IsSemanticFallbackAllowed(req.semantic);

            // ── Names from entry ──
            req.name = entry.name;
            req.struct_name = entry.struct_name;
            req.glsl_type = entry.glsl_type;

            // ── Fallback names by semantic ──
            if (req.name.empty())
            {
                const char *default_name = GetDefaultDescriptorNameBySemantic(
                    req.semantic);
                if (default_name)
                    req.name = default_name;
            }
            if (req.struct_name.empty())
            {
                const char *default_struct = GetDefaultStructNameBySemantic(
                    req.semantic);
                if (default_struct)
                    req.struct_name = default_struct;
            }

            // ── SSBO id corrections (matching BuildShaderResourceSchema) ──
            if (req.semantic == DescriptorSemantic::MaterialDataSlotData
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(req.data_slot);
            }
            if (req.semantic
                    == DescriptorSemantic::MaterialTextureLayerTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(
                    static_cast<uint32>(req.texture_slot));
            }
            if (req.semantic
                    == DescriptorSemantic::MaterialDataIndexTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(req.data_slot);
            }

            out_schema.resources.push_back(std::move(req));
        }
        return true;
    }

    uint64 GetDescriptorContractHash(
        const DescriptorContract &contract,
        const uint64 module_manifest_hash) noexcept
    {
        if (!ValidateDescriptorContract(contract))
            return 0;

        ShaderInterfaceContract interface_contract{};
        interface_contract.descriptor_requirements.Reserve(
            static_cast<int>(contract.entries.size()));
        for (const DescriptorContractEntry &entry :
             contract.entries)
        {
            interface_contract.descriptor_requirements.Add(
                entry.canonical);
        }

        hgl::hash::FNV1aHasher64 h;
        h << module_manifest_hash
          << GetShaderInterfaceContractHash(interface_contract);
        return h;
    }
}
