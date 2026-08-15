#pragma once

#include<hgl/mtl/SerializedDescriptorEntry.h>
#include<hgl/graph/ShaderBufferSources.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <vector>
#include <string>
#include <utility>

namespace hgl::graph::mtl
{
    struct ShaderResourceSlot
    {
        uint64 logical_resource_id = 0;
        uint64 resource_schema_id = 0;
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSemanticLayer semantic_layer = DescriptorSemanticLayer::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32_t data_slot = DefaultMaterialDataSlot;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = MakeRecipeSSBOId(0);
        uint32_t stage_flags = 0;

        std::string name;
        std::string struct_name;
        std::string glsl_type;

        bool required = true;
        bool allow_fallback = false;
    };

    struct ShaderResourceSchema
    {
        std::vector<ShaderResourceSlot> resources;
    };

    inline bool IsSemanticRequired(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::SkyInfo:
        case DescriptorSemantic::MaterialTexture:
        case DescriptorSemantic::MaterialSampler:
        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialDataIndexTable:
            return false;
        default:
            return true;
        }
    }

    inline bool IsSemanticFallbackAllowed(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::SkyInfo:
        case DescriptorSemantic::MaterialTexture:
        case DescriptorSemantic::MaterialSampler:
        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialDataIndexTable:
            return true;
        default:
            return false;
        }
    }

    // Whether a program's resource schema requires per-instance runtime rows:
    // a MaterialDataIndexTable / MaterialTextureLayerTable / MaterialDataSlotData
    // descriptor must be fed from per-batch row buffers keyed by the entity's own
    // data_index, rather than a static binding. Shared by RenderPrimitiveCollectSystem
    // and PrimitiveBatchPipeline so both agree on the same contract.
    inline bool MaterialRequiresRecipeRuntimeRows(const ShaderResourceSchema &schema)
    {
        for (const auto &req : schema.resources)
        {
            switch (req.semantic)
            {
            case DescriptorSemantic::MaterialDataSlotData:
            case DescriptorSemantic::MaterialDataIndexTable:
            case DescriptorSemantic::MaterialTextureLayerTable:
                return true;
            default:
                break;
            }
        }

        return false;
    }

    inline DescriptorSetType GetExpectedSetType(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo:
            return DescriptorSetType::Scene;

        case DescriptorSemantic::CameraInfo:
        case DescriptorSemantic::SkyInfo:
            return DescriptorSetType::Scene;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::LocalToWorldIndexTable:
            return DescriptorSetType::Transform;

        case DescriptorSemantic::MaterialColorPalette:
        case DescriptorSemantic::MaterialDataSlotData:
        case DescriptorSemantic::MaterialTexture:
        case DescriptorSemantic::MaterialSampler:
        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialDataIndexTable:
            return DescriptorSetType::Material;

        default:
            return DescriptorSetType::Unknow;
        }
    }

    inline const char *GetDescriptorKindName(const DescriptorKind kind)
    {
        switch (kind)
        {
        case DescriptorKind::UBO: return "UBO";
        case DescriptorKind::SSBO: return "SSBO";
        case DescriptorKind::Texture: return "Texture";
        case DescriptorKind::TextureSampler: return "TextureSampler";
        }

        return "Unknown";
    }

    inline DescriptorSemanticLayer NormalizeSemanticLayer(const SerializedDescriptorEntry &entry)
    {
        if (entry.semantic_layer != DescriptorSemanticLayer::Unknown)
            return entry.semantic_layer;

        const DescriptorSemanticLayer mapped = GetDescriptorSemanticLayer(entry.semantic);
        if (mapped != DescriptorSemanticLayer::Unknown)
            return mapped;

        // Semantics that can legally map to either UBO/SSBO remain kind-driven.
        if (entry.semantic == DescriptorSemantic::LocalToWorld
         || entry.semantic == DescriptorSemantic::MaterialDataSlotData)
        {
            switch (entry.kind)
            {
            case DescriptorKind::UBO: return DescriptorSemanticLayer::UBO;
            case DescriptorKind::SSBO: return DescriptorSemanticLayer::SSBO;
            default: return DescriptorSemanticLayer::Unknown;
            }
        }

        return DescriptorSemanticLayer::Unknown;
    }

    inline const char *GetDefaultDescriptorNameBySemantic(const DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo: return SBS_ViewportInfo.name;
        case DescriptorSemantic::CameraInfo: return SBS_CameraInfo.name;
        case DescriptorSemantic::SkyInfo: return SBS_SkyInfo.name;
        case DescriptorSemantic::LocalToWorld: return SBS_LocalToWorld.name;
        case DescriptorSemantic::LocalToWorldIndexTable: return SBS_LocalToWorldIndexRows.name;
        case DescriptorSemantic::MaterialDataSlotData: return DefaultMaterialDataSlotName;
        case DescriptorSemantic::MaterialColorPalette: return SBS_ColorPalette.name;
        case DescriptorSemantic::MaterialTextureLayerTable: return SBS_MaterialTextureLayerRows.name;
        case DescriptorSemantic::MaterialDataIndexTable: return SBS_MaterialDataIndexRows.name;
        default: return nullptr;
        }
    }

    inline const char *GetDefaultStructNameBySemantic(const DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo: return SBS_ViewportInfo.struct_name;
        case DescriptorSemantic::CameraInfo: return SBS_CameraInfo.struct_name;
        case DescriptorSemantic::SkyInfo: return SBS_SkyInfo.struct_name;
        case DescriptorSemantic::LocalToWorld: return SBS_LocalToWorld.struct_name;
        case DescriptorSemantic::LocalToWorldIndexTable: return SBS_LocalToWorldIndexRows.struct_name;
        case DescriptorSemantic::MaterialDataSlotData: return nullptr;
        case DescriptorSemantic::MaterialColorPalette: return SBS_ColorPalette.struct_name;
        case DescriptorSemantic::MaterialTextureLayerTable: return SBS_MaterialTextureLayerRows.struct_name;
        case DescriptorSemantic::MaterialDataIndexTable: return SBS_MaterialDataIndexRows.struct_name;
        default: return nullptr;
        }
    }

    inline ShaderResourceSchema BuildShaderResourceSchema(const SerializedDescriptorEntry *descriptor_entries,
                                                const uint32_t descriptor_entry_count)
    {
        ShaderResourceSchema contract;
        if (!descriptor_entries || descriptor_entry_count == 0)
            return contract;

        contract.resources.reserve(descriptor_entry_count);

        for (uint32_t i = 0; i < descriptor_entry_count; ++i)
        {
            const SerializedDescriptorEntry &entry = descriptor_entries[i];

            ShaderResourceSlot req;
            req.semantic = entry.semantic;
            req.semantic_layer = NormalizeSemanticLayer(entry);
            req.set_type = entry.set_type;
            req.kind = entry.kind;
            req.texture_slot = entry.texture_slot;
            req.data_slot = entry.data_slot;
            req.ssbo_type = entry.ssbo_type;
            req.ssbo_id = entry.ssbo_id;
            req.stage_flags = entry.stage_flags;
            req.name = entry.name ? entry.name : "";
            req.struct_name = entry.struct_name ? entry.struct_name : "";
            req.glsl_type = entry.glsl_type ? entry.glsl_type : "";
            req.required = entry.has_requirement_policy
                ? entry.required : IsSemanticRequired(req.semantic);
            req.allow_fallback = entry.has_requirement_policy
                ? entry.allow_fallback : IsSemanticFallbackAllowed(req.semantic);

            if (req.name.empty())
            {
                const char *default_name = GetDefaultDescriptorNameBySemantic(req.semantic);
                if (default_name)
                    req.name = default_name;
            }

            if (req.struct_name.empty())
            {
                const char *default_struct = GetDefaultStructNameBySemantic(req.semantic);
                if (default_struct)
                    req.struct_name = default_struct;
            }

            if (req.semantic == DescriptorSemantic::MaterialTexture
             || req.semantic == DescriptorSemantic::MaterialSampler)
            {
                req.texture_slot = entry.texture_slot;
            }

            if (req.semantic == DescriptorSemantic::MaterialDataSlotData)
            {
                req.data_slot = entry.data_slot;
                req.ssbo_type = entry.ssbo_type;
                if (req.ssbo_type == SSBOType::UserDefined)
                    req.ssbo_type = SSBOType::PBRSurface;
                if (req.ssbo_id == MakeRecipeSSBOId(0))
                    req.ssbo_id = MakeRecipeSSBOId(req.data_slot);
            }

            if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             && req.ssbo_type == SSBOType::UserDefined)
            {
                req.ssbo_type = SSBOType::TextureLayer;
            }
            if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(static_cast<uint32_t>(req.texture_slot));
            }

            if (req.semantic == DescriptorSemantic::MaterialDataIndexTable
             && req.ssbo_type == SSBOType::UserDefined)
            {
                req.ssbo_type = SSBOType::MaterialDataIndexTable;
            }
            if (req.semantic == DescriptorSemantic::MaterialDataIndexTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(req.data_slot);
            }

            if (req.semantic == DescriptorSemantic::LocalToWorldIndexTable
             && req.ssbo_type == SSBOType::UserDefined)
            {
                req.ssbo_type = SSBOType::TransformIndexRows;
            }

            if (!req.name.empty())
            {
                hgl::hash::FNV1aHasher64 h;
                h << req.name;
                req.logical_resource_id = h;
            }
            else
            {
                hgl::hash::FNV1aHasher64 h;

                h << req.semantic
                  << req.semantic_layer
                  << req.set_type
                  << req.kind
                  << req.texture_slot
                  << req.data_slot
                  << req.ssbo_type;

                req.logical_resource_id = h;
            }

            const std::string &schema_name =
                !req.struct_name.empty() ? req.struct_name : req.glsl_type;
            if (!schema_name.empty())
            {
                hgl::hash::FNV1aHasher64 h;
                h << schema_name;
                req.resource_schema_id = h;
            }
            else
            {
                hgl::hash::FNV1aHasher64 h;

                h << req.kind
                  << req.ssbo_type;

                req.resource_schema_id = h;
            }

            contract.resources.push_back(req);
        }

        return contract;
    }

    inline uint64 HashShaderResourceSchema(
        const ShaderResourceSchema &schema) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        constexpr uint32 contract_version = 4u;
        h << contract_version
          << static_cast<uint32>(schema.resources.size());

        for (const auto &req : schema.resources)
        {
            h << req.logical_resource_id
              << req.resource_schema_id
              << req.semantic
              << req.semantic_layer
              << req.set_type
              << req.kind
              << req.texture_slot
              << req.data_slot
              << req.ssbo_type
              << req.ssbo_id
              << req.stage_flags
              << req.required
              << req.allow_fallback;

            h << req.name
              << req.struct_name
              << req.glsl_type;
        }

        return h;
    }

    inline const char *GetDescriptorSemanticName(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::Unknown:          return "Unknown";
        case DescriptorSemantic::ViewportInfo:     return "ViewportInfo";
        case DescriptorSemantic::CameraInfo:       return "CameraInfo";
        case DescriptorSemantic::SkyInfo:          return "SkyInfo";
        case DescriptorSemantic::LocalToWorld:     return "LocalToWorld";
        case DescriptorSemantic::LocalToWorldIndexTable: return "LocalToWorldIndexTable";
        case DescriptorSemantic::MaterialColorPalette: return "MaterialColorPalette";
        case DescriptorSemantic::MaterialDataSlotData: return "MaterialDataSlotData";
        case DescriptorSemantic::MaterialTexture:  return "MaterialTexture";
        case DescriptorSemantic::MaterialSampler:  return "MaterialSampler";
        case DescriptorSemantic::MaterialTextureLayerTable: return "MaterialTextureLayerTable";
        case DescriptorSemantic::MaterialDataIndexTable: return "MaterialDataIndexTable";
        case DescriptorSemantic::Custom:           return "Custom";
        }

        return "Unknown";
    }

    inline bool ValidateShaderResourceSchema(const ShaderResourceSchema &schema, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        auto BuildEntryContext = [](const ShaderResourceSlot &req) -> std::string
        {
            std::string message = "semantic=";
            message += GetDescriptorSemanticName(req.semantic);
            message += ", layer=";
            message += GetDescriptorSemanticLayerName(req.semantic_layer);
            message += ", kind=";
            message += GetDescriptorKindName(req.kind);

            message += ", name=";
            const char *entry_name = req.name.empty() ? "<unnamed>" : req.name.c_str();
            message += entry_name;
            return message;
        };

        for (const ShaderResourceSlot &req : schema.resources)
        {
            const std::string context = BuildEntryContext(req);

            if (req.logical_resource_id == 0
             || req.resource_schema_id == 0)
            {
                diagnostics.emplace_back(
                    "Descriptor canonical resource identity is empty ("
                    + context + ").");
                continue;
            }

            if (req.semantic == DescriptorSemantic::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic is Unknown; every descriptor must use an explicit semantic enum (" + context + ").");
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
            {
                diagnostics.emplace_back("Descriptor semantic is Custom; runtime schema requires concrete semantic enums (" + context + ").");
                continue;
            }

            if (req.semantic_layer == DescriptorSemanticLayer::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic layer is Unknown; S1 requires typed UBO/SSBO/Texture/Sampler layers (" + context + ").");
                continue;
            }

            if (req.name.empty())
            {
                diagnostics.emplace_back("Descriptor resource name is empty; every resource must have a canonical name (" + context + ").");
                continue;
            }

            if (req.stage_flags == 0)
            {
                diagnostics.emplace_back(
                    "Descriptor stage visibility is empty (" + context + ").");
                continue;
            }

            if ((req.kind == DescriptorKind::Texture
              || req.kind == DescriptorKind::TextureSampler)
             && req.glsl_type.empty())
            {
                diagnostics.emplace_back("Texture resource GLSL type is empty (" + context + ").");
                continue;
            }

            const DescriptorSetType expected_set = GetExpectedSetType(req.semantic);
            if (expected_set != DescriptorSetType::Unknow && expected_set != req.set_type)
            {
                std::string message = "Descriptor semantic set mismatch: ";
                message += context;
                message += ", expected set=";
                message += GetDescriptorSetTypeName(expected_set);
                message += ", actual set=";
                message += GetDescriptorSetTypeName(req.set_type);
                diagnostics.push_back(std::move(message));
            }

            const bool layer_kind_mismatch =
                   (req.semantic_layer == DescriptorSemanticLayer::UBO && req.kind != DescriptorKind::UBO)
                || (req.semantic_layer == DescriptorSemanticLayer::SSBO && req.kind != DescriptorKind::SSBO)
                || (req.semantic_layer == DescriptorSemanticLayer::Texture && req.kind != DescriptorKind::Texture)
                || (req.semantic_layer == DescriptorSemanticLayer::Sampler && req.kind != DescriptorKind::TextureSampler);

            if (layer_kind_mismatch)
            {
                std::string message = "Descriptor semantic-kind mismatch: ";
                message += context;
                diagnostics.push_back(std::move(message));
                continue;
            }

            const bool requires_texture_slot =
                req.semantic == DescriptorSemantic::MaterialTexture
             || req.semantic == DescriptorSemantic::MaterialSampler;
            if (requires_texture_slot)
            {
                const auto slot_value = static_cast<uint32_t>(req.texture_slot);
                const auto slot_limit = static_cast<uint32_t>(TextureSlot::RANGE_SIZE);
                if (slot_value >= slot_limit)
                {
                    std::string message = "Descriptor texture_slot is invalid for texture/sampler semantic: ";
                    message += context;
                    diagnostics.push_back(std::move(message));
                    continue;
                }
            }

            const bool requires_data_ssbo =
                req.semantic == DescriptorSemantic::MaterialDataSlotData
             || req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             || req.semantic == DescriptorSemantic::MaterialDataIndexTable;
            if (requires_data_ssbo)
            {
                if (req.data_slot >= MaxMaterialDataSlotsPerMaterial)
                {
                    std::string message = "Descriptor data_slot is invalid for material SSBO semantic: ";
                    message += context;
                    diagnostics.push_back(std::move(message));
                    continue;
                }

                if (req.ssbo_type == SSBOType::UserDefined)
                {
                    std::string message = "Descriptor ssbo_type is UserDefined for material SSBO semantic; explicit/default-resolved SSBO type is required: ";
                    message += context;
                    diagnostics.push_back(std::move(message));
                    continue;
                }
            }
        }

        for (size_t i = 0; i < schema.resources.size(); ++i)
        {
            const ShaderResourceSlot &lhs = schema.resources[i];
            for (size_t j = i + 1; j < schema.resources.size(); ++j)
            {
                const ShaderResourceSlot &rhs = schema.resources[j];
                const bool same_name =
                    lhs.name == rhs.name;
                const bool same_logical_resource =
                    lhs.logical_resource_id == rhs.logical_resource_id;
                const bool same_semantic_key =
                    lhs.semantic == rhs.semantic
                 && lhs.texture_slot == rhs.texture_slot
                 && lhs.data_slot == rhs.data_slot;

                if (!same_name
                 && !same_logical_resource
                 && !same_semantic_key)
                    continue;

                const bool same_identity =
                    same_name
                 && same_logical_resource
                 && lhs.resource_schema_id == rhs.resource_schema_id
                 && lhs.semantic == rhs.semantic
                 && lhs.semantic_layer == rhs.semantic_layer
                 && lhs.set_type == rhs.set_type
                 && lhs.kind == rhs.kind
                 && lhs.texture_slot == rhs.texture_slot
                 && lhs.data_slot == rhs.data_slot
                 && lhs.ssbo_type == rhs.ssbo_type
                 && lhs.ssbo_id == rhs.ssbo_id
                 && lhs.stage_flags == rhs.stage_flags
                 && lhs.glsl_type == rhs.glsl_type;

                std::string message = same_identity
                    ? "Duplicate material resource requirement: "
                    : "Conflicting material resource requirements: ";
                message += BuildEntryContext(lhs);
                message += " vs ";
                message += BuildEntryContext(rhs);
                diagnostics.push_back(std::move(message));
            }
        }

        return diagnostics.empty();
    }
}
