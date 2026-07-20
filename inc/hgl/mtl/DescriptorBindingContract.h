#pragma once

#include<hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/mtl/UBOCommon.h>
#include<vector>
#include<string>

namespace hgl::graph::mtl
{
    struct DescriptorRequirement
    {
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSemanticLayer semantic_layer = DescriptorSemanticLayer::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        DataSlot data_slot = DataSlot::PBRSurface;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = MakeRecipeSSBOId(0);

        const char *name = nullptr;
        const char *struct_name = nullptr;
        const char *glsl_type = nullptr;

        bool required = true;
        bool allow_fallback = false;
    };

    struct BindingContract
    {
        std::vector<DescriptorRequirement> requirements;
    };

    inline bool IsSemanticRequired(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::SkyCubemapSampler:
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
        case DescriptorSemantic::SkyCubemapSampler:
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

    inline DescriptorSetType GetExpectedSetType(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo:
            return DescriptorSetType::Scene;

        case DescriptorSemantic::CameraInfo:
        case DescriptorSemantic::SkyInfo:
        case DescriptorSemantic::SkyCubemapSampler:
            return DescriptorSetType::Scene;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::LocalToWorldIndexTable:
            return DescriptorSetType::Transform;

        case DescriptorSemantic::MaterialColorPalette:
        case DescriptorSemantic::MaterialInstance:
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

    inline DescriptorSemanticLayer NormalizeSemanticLayer(const FixedDescriptorEntry &entry)
    {
        if (entry.semantic_layer != DescriptorSemanticLayer::Unknown)
            return entry.semantic_layer;

        const DescriptorSemanticLayer mapped = GetDescriptorSemanticLayer(entry.semantic);
        if (mapped != DescriptorSemanticLayer::Unknown)
            return mapped;

        // Legacy semantics that can legally map to either UBO/SSBO remain kind-driven.
        if (entry.semantic == DescriptorSemantic::LocalToWorld
         || entry.semantic == DescriptorSemantic::MaterialInstance)
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
        case DescriptorSemantic::MaterialInstance: return SBS_MaterialInstance.name;
        case DescriptorSemantic::MaterialColorPalette: return SBS_ColorPattle.name;
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
        case DescriptorSemantic::MaterialInstance: return SBS_MaterialInstance.struct_name;
        case DescriptorSemantic::MaterialColorPalette: return SBS_ColorPattle.struct_name;
        case DescriptorSemantic::MaterialTextureLayerTable: return SBS_MaterialTextureLayerRows.struct_name;
        case DescriptorSemantic::MaterialDataIndexTable: return SBS_MaterialDataIndexRows.struct_name;
        default: return nullptr;
        }
    }

    inline BindingContract BuildBindingContract(const FixedDescriptorEntry *descriptor_entries,
                                                const uint32_t descriptor_entry_count)
    {
        BindingContract contract;
        if (!descriptor_entries || descriptor_entry_count == 0)
            return contract;

        contract.requirements.reserve(descriptor_entry_count);

        for (uint32_t i = 0; i < descriptor_entry_count; ++i)
        {
            const FixedDescriptorEntry &entry = descriptor_entries[i];

            DescriptorRequirement req;
            req.semantic = entry.semantic;
            req.semantic_layer = NormalizeSemanticLayer(entry);
            req.set_type = entry.set_type;
            req.kind = entry.kind;
            req.texture_slot = entry.texture_slot;
            req.data_slot = entry.data_slot;
            req.ssbo_type = entry.ssbo_type;
            req.ssbo_id = entry.ssbo_id;
            req.name = entry.name;
            req.struct_name = entry.struct_name;
            req.glsl_type = entry.glsl_type;
            req.required = IsSemanticRequired(req.semantic);
            req.allow_fallback = IsSemanticFallbackAllowed(req.semantic);

            if (!req.name || !*req.name)
                req.name = GetDefaultDescriptorNameBySemantic(req.semantic);

            if (!req.struct_name || !*req.struct_name)
                req.struct_name = GetDefaultStructNameBySemantic(req.semantic);

            if (req.semantic == DescriptorSemantic::MaterialTexture
             || req.semantic == DescriptorSemantic::MaterialSampler)
            {
                req.texture_slot = entry.texture_slot;
            }

            if (req.semantic == DescriptorSemantic::MaterialInstance)
            {
                req.data_slot = entry.data_slot;
                req.ssbo_type = (entry.ssbo_type == SSBOType::UserDefined)
                              ? DefaultSSBOTypeForDataSlot(req.data_slot)
                              : entry.ssbo_type;
                if (req.ssbo_id == MakeRecipeSSBOId(0))
                    req.ssbo_id = MakeRecipeSSBOId(static_cast<uint32_t>(req.data_slot));
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
                req.ssbo_type = SSBOType::DataIndex;
            }
            if (req.semantic == DescriptorSemantic::MaterialDataIndexTable
             && req.ssbo_id == MakeRecipeSSBOId(0))
            {
                req.ssbo_id = MakeRecipeSSBOId(static_cast<uint32_t>(req.data_slot));
            }

            if (req.semantic == DescriptorSemantic::LocalToWorldIndexTable
             && req.ssbo_type == SSBOType::UserDefined)
            {
                req.ssbo_type = SSBOType::TransformIndexRows;
            }

            contract.requirements.push_back(req);
        }

        return contract;
    }

    inline const char *GetDescriptorSemanticName(DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::Unknown:          return "Unknown";
        case DescriptorSemantic::ViewportInfo:     return "ViewportInfo";
        case DescriptorSemantic::CameraInfo:       return "CameraInfo";
        case DescriptorSemantic::SkyInfo:          return "SkyInfo";
        case DescriptorSemantic::SkyCubemapSampler:return "SkyCubemapSampler";
        case DescriptorSemantic::LocalToWorld:     return "LocalToWorld";
        case DescriptorSemantic::LocalToWorldIndexTable: return "LocalToWorldIndexTable";
        case DescriptorSemantic::MaterialColorPalette: return "MaterialColorPalette";
        case DescriptorSemantic::MaterialInstance: return "MaterialInstance";
        case DescriptorSemantic::MaterialTexture:  return "MaterialTexture";
        case DescriptorSemantic::MaterialSampler:  return "MaterialSampler";
        case DescriptorSemantic::MaterialTextureLayerTable: return "MaterialTextureLayerTable";
        case DescriptorSemantic::MaterialDataIndexTable: return "MaterialDataIndexTable";
        case DescriptorSemantic::Custom:           return "Custom";
        }

        return "Unknown";
    }

    inline bool ValidateBindingContract(const BindingContract &contract, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        auto BuildEntryContext = [](const DescriptorRequirement &req) -> std::string
        {
            std::string message = "semantic=";
            message += GetDescriptorSemanticName(req.semantic);
            message += ", layer=";
            message += GetDescriptorSemanticLayerName(req.semantic_layer);
            message += ", kind=";
            message += GetDescriptorKindName(req.kind);

            message += ", name=";
            const char *entry_name = (req.name && *req.name) ? req.name : "<unnamed>";
            message += entry_name;
            return message;
        };

        for (const DescriptorRequirement &req : contract.requirements)
        {
            const std::string context = BuildEntryContext(req);

            if (req.semantic == DescriptorSemantic::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic is Unknown; every descriptor must use an explicit semantic enum (" + context + ").");
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
            {
                diagnostics.emplace_back("Descriptor semantic is Custom; runtime contract requires concrete semantic enums (" + context + ").");
                continue;
            }

            if (req.semantic_layer == DescriptorSemanticLayer::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic layer is Unknown; S1 requires typed UBO/SSBO/Texture/Sampler layers (" + context + ").");
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
                req.semantic == DescriptorSemantic::MaterialInstance
             || req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             || req.semantic == DescriptorSemantic::MaterialDataIndexTable;
            if (requires_data_ssbo)
            {
                const auto data_slot_value = static_cast<uint32_t>(req.data_slot);
                const auto data_slot_limit = static_cast<uint32_t>(DataSlot::RANGE_SIZE);
                if (data_slot_value >= data_slot_limit)
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

        return diagnostics.empty();
    }
}
