#pragma once

#include<hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/graph/ShaderBufferSources.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>
#include <vector>
#include <string>
#include <utility>

namespace hgl::graph::mtl
{
    struct MaterialResourceRequirement
    {
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSemanticLayer semantic_layer = DescriptorSemanticLayer::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32_t data_slot = DefaultMaterialDataSlot;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = MakeRecipeSSBOId(0);
        uint32_t stage_flags = 0;

        const char *name = nullptr;
        const char *struct_name = nullptr;
        const char *glsl_type = nullptr;

        // Owned copies — set when name/struct_name comes from a runtime std::string
        // (e.g. generated from data_slot_decls) rather than a string literal.
        std::string owned_name;
        std::string owned_struct_name;
        std::string owned_glsl_type;

        bool required = true;
        bool allow_fallback = false;

        void RebindOwnedPointers()
        {
            if (!owned_name.empty())
                name = owned_name.c_str();

            if (!owned_struct_name.empty())
                struct_name = owned_struct_name.c_str();

            if (!owned_glsl_type.empty())
                glsl_type = owned_glsl_type.c_str();
        }

        MaterialResourceRequirement() = default;

        MaterialResourceRequirement(const MaterialResourceRequirement &rhs)
            : semantic(rhs.semantic)
            , semantic_layer(rhs.semantic_layer)
            , set_type(rhs.set_type)
            , kind(rhs.kind)
            , texture_slot(rhs.texture_slot)
            , data_slot(rhs.data_slot)
            , ssbo_type(rhs.ssbo_type)
            , ssbo_id(rhs.ssbo_id)
            , stage_flags(rhs.stage_flags)
            , name(rhs.name)
            , struct_name(rhs.struct_name)
            , glsl_type(rhs.glsl_type)
            , owned_name(rhs.owned_name)
            , owned_struct_name(rhs.owned_struct_name)
            , owned_glsl_type(rhs.owned_glsl_type)
            , required(rhs.required)
            , allow_fallback(rhs.allow_fallback)
        {
            RebindOwnedPointers();
        }

        MaterialResourceRequirement &operator=(const MaterialResourceRequirement &rhs)
        {
            if (this == &rhs)
                return *this;

            semantic = rhs.semantic;
            semantic_layer = rhs.semantic_layer;
            set_type = rhs.set_type;
            kind = rhs.kind;
            texture_slot = rhs.texture_slot;
            data_slot = rhs.data_slot;
            ssbo_type = rhs.ssbo_type;
            ssbo_id = rhs.ssbo_id;
            stage_flags = rhs.stage_flags;
            name = rhs.name;
            struct_name = rhs.struct_name;
            glsl_type = rhs.glsl_type;
            owned_name = rhs.owned_name;
            owned_struct_name = rhs.owned_struct_name;
            owned_glsl_type = rhs.owned_glsl_type;
            required = rhs.required;
            allow_fallback = rhs.allow_fallback;

            RebindOwnedPointers();
            return *this;
        }

        MaterialResourceRequirement(MaterialResourceRequirement &&rhs) noexcept
            : semantic(rhs.semantic)
            , semantic_layer(rhs.semantic_layer)
            , set_type(rhs.set_type)
            , kind(rhs.kind)
            , texture_slot(rhs.texture_slot)
            , data_slot(rhs.data_slot)
            , ssbo_type(rhs.ssbo_type)
            , ssbo_id(rhs.ssbo_id)
            , stage_flags(rhs.stage_flags)
            , name(rhs.name)
            , struct_name(rhs.struct_name)
            , glsl_type(rhs.glsl_type)
            , owned_name(std::move(rhs.owned_name))
            , owned_struct_name(std::move(rhs.owned_struct_name))
            , owned_glsl_type(std::move(rhs.owned_glsl_type))
            , required(rhs.required)
            , allow_fallback(rhs.allow_fallback)
        {
            RebindOwnedPointers();
        }

        MaterialResourceRequirement &operator=(MaterialResourceRequirement &&rhs) noexcept
        {
            if (this == &rhs)
                return *this;

            semantic = rhs.semantic;
            semantic_layer = rhs.semantic_layer;
            set_type = rhs.set_type;
            kind = rhs.kind;
            texture_slot = rhs.texture_slot;
            data_slot = rhs.data_slot;
            ssbo_type = rhs.ssbo_type;
            ssbo_id = rhs.ssbo_id;
            stage_flags = rhs.stage_flags;
            name = rhs.name;
            struct_name = rhs.struct_name;
            glsl_type = rhs.glsl_type;
            owned_name = std::move(rhs.owned_name);
            owned_struct_name = std::move(rhs.owned_struct_name);
            owned_glsl_type = std::move(rhs.owned_glsl_type);
            required = rhs.required;
            allow_fallback = rhs.allow_fallback;

            RebindOwnedPointers();
            return *this;
        }
    };

    struct MaterialResourceLayout
    {
        std::vector<MaterialResourceRequirement> requirements;
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

    inline DescriptorSemanticLayer NormalizeSemanticLayer(const FixedDescriptorEntry &entry)
    {
        if (entry.semantic_layer != DescriptorSemanticLayer::Unknown)
            return entry.semantic_layer;

        const DescriptorSemanticLayer mapped = GetDescriptorSemanticLayer(entry.semantic);
        if (mapped != DescriptorSemanticLayer::Unknown)
            return mapped;

        // Legacy semantics that can legally map to either UBO/SSBO remain kind-driven.
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

    inline MaterialResourceLayout BuildMaterialResourceLayout(const FixedDescriptorEntry *descriptor_entries,
                                                const uint32_t descriptor_entry_count)
    {
        MaterialResourceLayout contract;
        if (!descriptor_entries || descriptor_entry_count == 0)
            return contract;

        contract.requirements.reserve(descriptor_entry_count);

        for (uint32_t i = 0; i < descriptor_entry_count; ++i)
        {
            const FixedDescriptorEntry &entry = descriptor_entries[i];

            MaterialResourceRequirement req;
            req.semantic = entry.semantic;
            req.semantic_layer = NormalizeSemanticLayer(entry);
            req.set_type = entry.set_type;
            req.kind = entry.kind;
            req.texture_slot = entry.texture_slot;
            req.data_slot = entry.data_slot;
            req.ssbo_type = entry.ssbo_type;
            req.ssbo_id = entry.ssbo_id;
            req.stage_flags = entry.stage_flags;
            req.name = entry.name;
            req.struct_name = entry.struct_name;
            req.glsl_type = entry.glsl_type;
            req.required = entry.has_requirement_policy
                ? entry.required : IsSemanticRequired(req.semantic);
            req.allow_fallback = entry.has_requirement_policy
                ? entry.allow_fallback : IsSemanticFallbackAllowed(req.semantic);

            if (!req.name || !*req.name)
                req.name = GetDefaultDescriptorNameBySemantic(req.semantic);

            if (!req.struct_name || !*req.struct_name)
                req.struct_name = GetDefaultStructNameBySemantic(req.semantic);

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

            contract.requirements.push_back(req);
        }

        return contract;
    }

    inline uint64 HashMaterialResourceLayout(
        const MaterialResourceLayout &layout) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        constexpr uint32 contract_version = 3u;
        hash = hgl::hash::FNV1aAppendValueBytes(hash, contract_version);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(layout.requirements.size()));

        const auto append_string = [](uint64 current, const char *value) -> uint64
        {
            const uint32 length = value
                ? static_cast<uint32>(std::strlen(value)) : 0u;
            current = hgl::hash::FNV1aAppendValueBytes(current, length);
            if (length > 0)
                current = hgl::hash::FNV1aAppendBytes(current, value, length);
            return current;
        };

        for (const auto &req : layout.requirements)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.semantic_layer);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.set_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.kind);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.texture_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, req.stage_flags);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.required);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, req.allow_fallback);
            hash = append_string(hash, req.name);
            hash = append_string(hash, req.struct_name);
            hash = append_string(hash, req.glsl_type);
        }

        return hash;
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
        case DescriptorSemantic::MaterialDataSlotData: return "MaterialDataSlotData";
        case DescriptorSemantic::MaterialTexture:  return "MaterialTexture";
        case DescriptorSemantic::MaterialSampler:  return "MaterialSampler";
        case DescriptorSemantic::MaterialTextureLayerTable: return "MaterialTextureLayerTable";
        case DescriptorSemantic::MaterialDataIndexTable: return "MaterialDataIndexTable";
        case DescriptorSemantic::Custom:           return "Custom";
        }

        return "Unknown";
    }

    inline bool ValidateMaterialResourceLayout(const MaterialResourceLayout &contract, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        auto BuildEntryContext = [](const MaterialResourceRequirement &req) -> std::string
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

        for (const MaterialResourceRequirement &req : contract.requirements)
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

            if (!req.name || !*req.name)
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
             && (!req.glsl_type || !*req.glsl_type))
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

        for (size_t i = 0; i < contract.requirements.size(); ++i)
        {
            const MaterialResourceRequirement &lhs = contract.requirements[i];
            for (size_t j = i + 1; j < contract.requirements.size(); ++j)
            {
                const MaterialResourceRequirement &rhs = contract.requirements[j];
                const bool same_name =
                    lhs.name && rhs.name && std::strcmp(lhs.name, rhs.name) == 0;
                const bool same_semantic_key =
                    lhs.semantic == rhs.semantic
                 && lhs.texture_slot == rhs.texture_slot
                 && lhs.data_slot == rhs.data_slot;

                if (!same_name && !same_semantic_key)
                    continue;

                const bool same_identity =
                    same_name
                 && lhs.semantic == rhs.semantic
                 && lhs.semantic_layer == rhs.semantic_layer
                 && lhs.set_type == rhs.set_type
                 && lhs.kind == rhs.kind
                 && lhs.texture_slot == rhs.texture_slot
                 && lhs.data_slot == rhs.data_slot
                 && lhs.ssbo_type == rhs.ssbo_type
                 && lhs.ssbo_id == rhs.ssbo_id
                 && lhs.stage_flags == rhs.stage_flags
                 && ((!lhs.glsl_type && !rhs.glsl_type)
                  || (lhs.glsl_type && rhs.glsl_type
                   && std::strcmp(lhs.glsl_type, rhs.glsl_type) == 0));

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
