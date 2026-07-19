#pragma once

#include<hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/MaterializationPools.h>
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

    inline DescriptorSemanticLayer GetDescriptorLayerByKind(const DescriptorKind kind)
    {
        switch (kind)
        {
        case DescriptorKind::UBO: return DescriptorSemanticLayer::UBO;
        case DescriptorKind::SSBO: return DescriptorSemanticLayer::SSBO;
        case DescriptorKind::Texture: return DescriptorSemanticLayer::Texture;
        case DescriptorKind::TextureSampler: return DescriptorSemanticLayer::Sampler;
        default:
            return DescriptorSemanticLayer::Unknown;
        }
    }

    inline DescriptorSemanticLayer NormalizeSemanticLayer(const FixedDescriptorEntry &entry)
    {
        if (entry.semantic_layer != DescriptorSemanticLayer::Unknown)
            return entry.semantic_layer;

        const DescriptorSemanticLayer inferred = GetDescriptorSemanticLayer(entry.semantic);
        if (inferred != DescriptorSemanticLayer::Unknown)
            return inferred;

        if (entry.semantic == DescriptorSemantic::LocalToWorld
         || entry.semantic == DescriptorSemantic::MaterialInstance)
        {
            return GetDescriptorLayerByKind(entry.kind);
        }

        return DescriptorSemanticLayer::Unknown;
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
            req.name = entry.name;
            req.struct_name = entry.struct_name;
            req.glsl_type = entry.glsl_type;
            req.required = IsSemanticRequired(req.semantic);
            req.allow_fallback = IsSemanticFallbackAllowed(req.semantic);

            req.texture_slot = entry.texture_slot;
            req.data_slot = entry.data_slot;
            req.ssbo_type = entry.ssbo_type;

            if (req.semantic == DescriptorSemantic::MaterialInstance && req.ssbo_type == SSBOType::UserDefined)
                req.ssbo_type = DefaultSSBOTypeForDataSlot(req.data_slot);

            if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable && req.ssbo_type == SSBOType::UserDefined)
                req.ssbo_type = SSBOType::TextureLayer;

            if (req.semantic == DescriptorSemantic::MaterialDataIndexTable && req.ssbo_type == SSBOType::UserDefined)
                req.ssbo_type = SSBOType::DataIndex;

            if (req.semantic == DescriptorSemantic::LocalToWorldIndexTable && req.ssbo_type == SSBOType::UserDefined)
                req.ssbo_type = SSBOType::TransformIndexRows;

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

    inline const char *GetDescriptorKindName(DescriptorKind kind)
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

    inline void AppendDiagnosticContext(std::string &message, const DescriptorRequirement &req)
    {
        message += " [semantic=";
        message += GetDescriptorSemanticName(req.semantic);
        message += ", layer=";
        message += GetDescriptorSemanticLayerName(req.semantic_layer);
        message += ", kind=";
        message += GetDescriptorKindName(req.kind);

        if (req.name)
        {
            message += ", name=";
            message += req.name;
        }

        message += "]";
    }

    inline bool ValidateBindingContract(const BindingContract &contract, std::vector<std::string> &diagnostics)
    {
        diagnostics.clear();

        for (const DescriptorRequirement &req : contract.requirements)
        {
            if (req.semantic == DescriptorSemantic::Unknown)
            {
                std::string message = "Descriptor semantic is Unknown; every descriptor must use an explicit semantic enum.";
                AppendDiagnosticContext(message, req);
                diagnostics.emplace_back(std::move(message));
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
            {
                std::string message = "Descriptor semantic is Custom; runtime contract requires concrete semantic enums.";
                AppendDiagnosticContext(message, req);
                diagnostics.emplace_back(std::move(message));
                continue;
            }

            if (req.semantic_layer == DescriptorSemanticLayer::Unknown)
            {
                std::string message = "Descriptor semantic layer is Unknown; S1 requires explicit layered semantics.";
                AppendDiagnosticContext(message, req);
                diagnostics.emplace_back(std::move(message));
                continue;
            }

            const DescriptorSemanticLayer semantic_expected_layer = GetDescriptorSemanticLayer(req.semantic);
            if (semantic_expected_layer != DescriptorSemanticLayer::Unknown && req.semantic_layer != semantic_expected_layer)
            {
                std::string message = "Descriptor semantic layer mismatch: expected layer=";
                message += GetDescriptorSemanticLayerName(semantic_expected_layer);
                message += ", actual layer=";
                message += GetDescriptorSemanticLayerName(req.semantic_layer);
                AppendDiagnosticContext(message, req);
                diagnostics.emplace_back(std::move(message));
                continue;
            }

            const DescriptorSetType expected_set = GetExpectedSetType(req.semantic);
            if (expected_set != DescriptorSetType::Unknow && expected_set != req.set_type)
            {
                std::string message = "Descriptor semantic set mismatch: expected set=";
                message += GetDescriptorSetTypeName(expected_set);
                message += ", actual set=";
                message += GetDescriptorSetTypeName(req.set_type);
                AppendDiagnosticContext(message, req);
                diagnostics.push_back(std::move(message));
            }

            const DescriptorSemanticLayer kind_layer = GetDescriptorLayerByKind(req.kind);
            if (kind_layer != req.semantic_layer)
            {
                std::string message = "Descriptor semantic layer-kind mismatch: expected kind-layer=";
                message += GetDescriptorSemanticLayerName(req.semantic_layer);
                message += ", actual kind-layer=";
                message += GetDescriptorSemanticLayerName(kind_layer);
                AppendDiagnosticContext(message, req);
                diagnostics.push_back(std::move(message));
            }

            if (req.semantic == DescriptorSemantic::MaterialTexture
             || req.semantic == DescriptorSemantic::MaterialSampler)
            {
                if (!hgl::RangeCheck(req.texture_slot))
                {
                    std::string message = "Material texture semantic requires valid texture_slot.";
                    AppendDiagnosticContext(message, req);
                    diagnostics.push_back(std::move(message));
                }
            }

            if (req.semantic == DescriptorSemantic::MaterialInstance
             || req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             || req.semantic == DescriptorSemantic::MaterialDataIndexTable)
            {
                if (!hgl::RangeCheck(req.data_slot))
                {
                    std::string message = "Material SSBO semantic requires valid data_slot.";
                    AppendDiagnosticContext(message, req);
                    diagnostics.push_back(std::move(message));
                }

                if (req.ssbo_type == SSBOType::UserDefined)
                {
                    std::string message = "Material SSBO semantic requires explicit ssbo_type (not UserDefined).";
                    AppendDiagnosticContext(message, req);
                    diagnostics.push_back(std::move(message));
                }
            }
        }

        return diagnostics.empty();
    }
}
