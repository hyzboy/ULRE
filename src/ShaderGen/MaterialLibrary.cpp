#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph::mtl{
namespace
{
    struct BaseMaterialInfoRegistryEntry
    {
        bool has_preset = false;
        MaterialPreset preset = MaterialPreset::VertexColor2D;
        BaseMaterialInfo bmi{};
    };

    std::vector<BaseMaterialInfoRegistryEntry> &GetBaseMaterialInfoRegistry()
    {
        static std::vector<BaseMaterialInfoRegistryEntry> registry;
        return registry;
    }
}

VkFormat ResolveMaterialVertexSemanticFormat(const MaterialCreateConfig *cfg, VertexSemantic semantic, VkFormat fallback_format)
{
    if(!cfg||!cfg->geometry_vertex_format)
        return fallback_format;

    const GeometryVertexAttributeFormat *attribute=cfg->geometry_vertex_format->Find(semantic);
    if(!attribute||attribute->format==VK_FORMAT_UNDEFINED)
        return fallback_format;

    return attribute->format;
}

VkFormat ResolveMaterialPositionFormat(const MaterialCreateConfig *cfg, VkFormat fallback_format)
{
    return ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Position, fallback_format);
}

void RegisterBaseMaterialInfo(const BaseMaterialInfo &bmi)
{
    BaseMaterialInfo normalized = bmi;
    if (normalized.bmi_id.empty())
    {
        if (!normalized.bmi_name.empty())
            normalized.bmi_id = normalized.bmi_name;
    }

    if (normalized.bmi_name.empty() && normalized.bmi_id.empty())
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if ((!normalized.bmi_id.empty() && entry.bmi.bmi_id == normalized.bmi_id)
         || (!normalized.bmi_name.empty() && entry.bmi.bmi_name == normalized.bmi_name))
        {
            entry.bmi = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.bmi = normalized;
    registry.emplace_back(std::move(entry));
}

void RegisterBaseMaterialInfo(const MaterialPreset preset, const BaseMaterialInfo &bmi)
{
    BaseMaterialInfo normalized = bmi;
    const char *preset_name = GetMaterialPresetName(preset);

    if (normalized.bmi_id.empty())
    {
        if (preset_name && *preset_name)
            normalized.bmi_id = preset_name;
    }

    if (normalized.bmi_name.empty())
    {
        if (preset_name && *preset_name)
            normalized.bmi_name = preset_name;
    }

    if (normalized.bmi_name.empty() && normalized.bmi_id.empty())
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if (entry.has_preset && entry.preset == preset)
        {
            entry.bmi = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.has_preset = true;
    entry.preset = preset;
    entry.bmi = normalized;
    registry.emplace_back(std::move(entry));
}

bool TryGetBaseMaterialInfoByBMIId(const std::string &bmi_id, BaseMaterialInfo &out_bmi)
{
    if (bmi_id.empty())
        return false;

    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.bmi.bmi_id == bmi_id)
        {
            out_bmi = entry.bmi;
            return true;
        }
    }

    return false;
}


bool TryGetBaseMaterialInfoByPreset(const MaterialPreset preset, BaseMaterialInfo &out_bmi)
{
    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.has_preset && entry.preset == preset)
        {
            out_bmi = entry.bmi;
            return true;
        }
    }

    return false;
}


const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return "VertexColor2D";
        case MaterialPreset::PureColor2D:           return "PureColor2D";
        case MaterialPreset::PureTexture2D:         return "PureTexture2D";
        case MaterialPreset::RectTexture2D:         return "RectTexture2D";
        case MaterialPreset::RectTexture2DArray:    return "RectTexture2DArray";
        case MaterialPreset::Text2D:                return "Text2D";
        case MaterialPreset::PureColor3D:           return "PureColor3D";
        case MaterialPreset::VertexColor3D:         return "VertexColor3D";
        case MaterialPreset::VertexLuminance3D:     return "VertexLuminance3D";
        case MaterialPreset::VertexPattleColor3D:   return "VertexPattleColor3D";
        case MaterialPreset::Gizmo3D:               return "Gizmo3D";
        case MaterialPreset::SkyMinimal:            return "SkyMinimal";
        case MaterialPreset::Standard:              return "Standard";
        case MaterialPreset::StandardTextureArray:  return "StandardTextureArray";
        case MaterialPreset::PBRColor3D:            return "PBRColor3D";
        default:                                    return nullptr;
    }
}


MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return CreateVertexColor2D      (profile,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::PureColor2D:           return CreatePureColor2D        (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::PureTexture2D:         return CreatePureTexture2D      (profile,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2D:         return CreateRectTexture2D      (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2DArray:    return CreateRectTexture2DArray (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::Text2D:                return CreateText2D             (profile,(const Text2DMaterialCreateConfig *)cfg);

        case MaterialPreset::PureColor3D:           return CreatePureColor3D        (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexColor3D:         return CreateVertexColor3D      (profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance3D:     return CreateVertexLuminance3D  (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexPattleColor3D:   return CreateVertexPattleColor3D(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::Gizmo3D:               return CreateGizmo3D            (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::SkyMinimal:            return CreateSkyMinimal         (profile,(const SkyMinimalCreateConfig *)cfg);
        case MaterialPreset::Standard:              return CreateStandard           (profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::StandardTextureArray:  return CreateStandardTextureArray(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::PBRColor3D:            return CreatePBRColor3D         (profile,(PBRColor3DMaterialCreateConfig *)cfg);

        default:                                    return nullptr;
    }
}
}//namespace hgl::graph::mtl