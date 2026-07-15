#pragma once

#include<hgl/mtl/FixedDescriptorEntry.h>
#include<vector>
#include<string>
#include<cstring>

namespace hgl::graph::mtl
{
    enum class DescriptorSemantic : uint8
    {
        Unknown = 0,

        ViewportInfo,
        CameraInfo,
        SkyInfo,

        LocalToWorld,
        LocalToWorldIndexTable,
        MaterialInstance,

        MaterialTexture,
        MaterialSampler,
        MaterialTextureLayerTable,
        MaterialDataIndexTable,

        Custom,
    };

    struct DescriptorRequirement
    {
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;

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

    inline bool _DBC_CStrEq(const char *lhs, const char *rhs)
    {
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

    inline bool _DBC_CStrContains(const char *text, const char *token)
    {
        return text && token && *token && std::strstr(text, token) != nullptr;
    }

    inline DescriptorSemantic InferDescriptorSemantic(const FixedDescriptorEntry &entry)
    {
        if (_DBC_CStrEq(entry.struct_name, "ViewportInfo") || _DBC_CStrEq(entry.name, "viewport"))
            return DescriptorSemantic::ViewportInfo;

        if (_DBC_CStrEq(entry.struct_name, "CameraInfo") || _DBC_CStrEq(entry.name, "camera"))
            return DescriptorSemantic::CameraInfo;

        if (_DBC_CStrEq(entry.struct_name, "SkyInfo") || _DBC_CStrEq(entry.name, "sky"))
            return DescriptorSemantic::SkyInfo;

        if (_DBC_CStrEq(entry.struct_name, "LocalToWorldData") || _DBC_CStrEq(entry.struct_name, "LocalToWorld") || _DBC_CStrEq(entry.name, "l2w"))
            return DescriptorSemantic::LocalToWorld;

        if (_DBC_CStrEq(entry.struct_name, "LocalToWorldIndexRows")
         || _DBC_CStrEq(entry.struct_name, "TransformIndexRows")
         || _DBC_CStrEq(entry.name, "l2w_index_rows")
         || _DBC_CStrEq(entry.name, "transform_index_rows"))
            return DescriptorSemantic::LocalToWorldIndexTable;

        if (_DBC_CStrEq(entry.struct_name, "MaterialInstanceData") || _DBC_CStrEq(entry.struct_name, "MaterialInstance") || _DBC_CStrEq(entry.name, "mtl"))
            return DescriptorSemantic::MaterialInstance;

        if (entry.kind == DescriptorKind::SSBO)
        {
            if (_DBC_CStrEq(entry.struct_name, "TextureLayerRow")
             || _DBC_CStrEq(entry.struct_name, "TextureLayerRows")
             || _DBC_CStrEq(entry.struct_name, "TextureLayerTable")
             || _DBC_CStrEq(entry.name, "texture_layer_rows")
             || _DBC_CStrEq(entry.name, "texture_layer_table")
             || _DBC_CStrContains(entry.name, "textureLayer"))
                return DescriptorSemantic::MaterialTextureLayerTable;

            if (_DBC_CStrEq(entry.struct_name, "DataIndexRow")
             || _DBC_CStrEq(entry.struct_name, "DataIndexRows")
             || _DBC_CStrEq(entry.struct_name, "DataIndexTable")
             || _DBC_CStrEq(entry.name, "data_index_rows")
             || _DBC_CStrEq(entry.name, "data_index_table")
             || _DBC_CStrContains(entry.name, "dataIndex"))
                return DescriptorSemantic::MaterialDataIndexTable;
        }

        if (entry.kind == DescriptorKind::Texture)
            return DescriptorSemantic::MaterialTexture;

        if (entry.kind == DescriptorKind::TextureSampler)
            return DescriptorSemantic::MaterialSampler;

        if (entry.struct_name || entry.name)
            return DescriptorSemantic::Custom;

        return DescriptorSemantic::Unknown;
    }

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
            req.semantic = InferDescriptorSemantic(entry);
            req.set_type = entry.set_type;
            req.kind = entry.kind;
            req.name = entry.name;
            req.struct_name = entry.struct_name;
            req.glsl_type = entry.glsl_type;
            req.required = IsSemanticRequired(req.semantic);
            req.allow_fallback = IsSemanticFallbackAllowed(req.semantic);

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
        case DescriptorSemantic::LocalToWorld:     return "LocalToWorld";
        case DescriptorSemantic::LocalToWorldIndexTable: return "LocalToWorldIndexTable";
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

        for (const DescriptorRequirement &req : contract.requirements)
        {
            if (req.semantic == DescriptorSemantic::Unknown)
            {
                diagnostics.emplace_back("Descriptor semantic is Unknown; add explicit semantic mapping or mark as Custom.");
                continue;
            }

            if (req.semantic == DescriptorSemantic::Custom)
                continue;

            const DescriptorSetType expected_set = GetExpectedSetType(req.semantic);
            if (expected_set != DescriptorSetType::Unknow && expected_set != req.set_type)
            {
                std::string message = "Descriptor semantic set mismatch: semantic=";
                message += GetDescriptorSemanticName(req.semantic);
                message += ", expected set=";
                message += GetDescriptorSetTypeName(expected_set);
                message += ", actual set=";
                message += GetDescriptorSetTypeName(req.set_type);

                if (req.name)
                {
                    message += ", name=";
                    message += req.name;
                }

                diagnostics.push_back(std::move(message));
            }
        }

        return diagnostics.empty();
    }
}
