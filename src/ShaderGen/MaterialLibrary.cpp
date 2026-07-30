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
        BuiltinMaterialCreatorID preset = BuiltinMaterialCreatorID::VertexColor2D;
        MaterialDefinition bmi{};
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

void RegisterMaterialDefinition(const MaterialDefinition &bmi)
{
    MaterialDefinition normalized = bmi;
    if (normalized.definition_id.empty())
    {
        if (!normalized.definition_name.empty())
            normalized.definition_id = normalized.definition_name;
    }

    if (normalized.definition_name.empty() && normalized.definition_id.empty())
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if ((!normalized.definition_id.empty() && entry.bmi.definition_id == normalized.definition_id)
         || (!normalized.definition_name.empty() && entry.bmi.definition_name == normalized.definition_name))
        {
            entry.bmi = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.bmi = normalized;
    registry.emplace_back(std::move(entry));
}

void RegisterMaterialDefinition(const BuiltinMaterialCreatorID preset, const MaterialDefinition &bmi)
{
    MaterialDefinition normalized = bmi;
    const char *preset_name = GetBuiltinMaterialCreatorIDName(preset);

    if (normalized.definition_id.empty())
    {
        if (preset_name && *preset_name)
            normalized.definition_id = preset_name;
    }

    if (normalized.definition_name.empty())
    {
        if (preset_name && *preset_name)
            normalized.definition_name = preset_name;
    }

    if (normalized.definition_name.empty() && normalized.definition_id.empty())
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

bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_bmi)
{
    if (mtl_def_id.empty())
        return false;

    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.bmi.definition_id == mtl_def_id)
        {
            out_bmi = entry.bmi;
            return true;
        }
    }

    return false;
}


bool TryGetMaterialDefinitionByBuiltinMaterialCreatorID(const BuiltinMaterialCreatorID preset, MaterialDefinition &out_bmi)
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


const char *GetBuiltinMaterialCreatorIDName(const BuiltinMaterialCreatorID mtl_id)
{
    switch(mtl_id)
    {
        case BuiltinMaterialCreatorID::VertexColor2D:         return "VertexColor2D";
        case BuiltinMaterialCreatorID::PureColor2D:           return "PureColor2D";
        case BuiltinMaterialCreatorID::PureTexture2D:         return "PureTexture2D";
        case BuiltinMaterialCreatorID::RectTexture2D:         return "RectTexture2D";
        case BuiltinMaterialCreatorID::RectTexture2DArray:    return "RectTexture2DArray";
        case BuiltinMaterialCreatorID::Text2D:                return "Text2D";
        case BuiltinMaterialCreatorID::PureColor3D:           return "PureColor3D";
        case BuiltinMaterialCreatorID::VertexColor3D:         return "VertexColor3D";
        case BuiltinMaterialCreatorID::VertexLuminance3D:     return "VertexLuminance3D";
        case BuiltinMaterialCreatorID::VertexPattleColor3D:   return "VertexPattleColor3D";
        case BuiltinMaterialCreatorID::Gizmo3D:               return "Gizmo3D";
        case BuiltinMaterialCreatorID::SkyMinimal:            return "SkyMinimal";
        case BuiltinMaterialCreatorID::Standard:              return "Standard";
        case BuiltinMaterialCreatorID::StandardTextureArray:  return "StandardTextureArray";
        case BuiltinMaterialCreatorID::PBRColor3D:            return "PBRColor3D";
        default:                                    return nullptr;
    }
}


ShaderProgramBuildSpec *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const BuiltinMaterialCreatorID mtl_id,
                                             MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    switch(mtl_id)
    {
        case BuiltinMaterialCreatorID::VertexColor2D:         return CreateVertexColor2D      (profile,(const Material2DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::PureColor2D:           return CreatePureColor2D        (profile,(Material2DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::PureTexture2D:         return CreatePureTexture2D      (profile,(const Material2DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::RectTexture2D:         return CreateRectTexture2D      (profile,(Material2DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::RectTexture2DArray:    return CreateRectTexture2DArray (profile,(Material2DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::Text2D:                return CreateText2D             (profile,(const Text2DMaterialCreateConfig *)cfg);

        case BuiltinMaterialCreatorID::PureColor3D:           return CreatePureColor3D        (profile,(Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::VertexColor3D:         return CreateVertexColor3D      (profile,(const Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::VertexLuminance3D:     return CreateVertexLuminance3D  (profile,(Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::VertexPattleColor3D:   return CreateVertexPattleColor3D(profile,(const Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::Gizmo3D:               return CreateGizmo3D            (profile,(Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::SkyMinimal:            return CreateSkyMinimal         (profile,(const SkyMinimalCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::Standard:              return CreateStandard           (profile,(const Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::StandardTextureArray:  return CreateStandardTextureArray(profile,(const Material3DCreateConfig *)cfg);
        case BuiltinMaterialCreatorID::PBRColor3D:            return CreatePBRColor3D         (profile,(PBRColor3DMaterialCreateConfig *)cfg);

        default:                                    return nullptr;
    }
}

static void ApplyBuildRequestToLegacyConfig(MaterialCreateConfig &cfg,const MaterialDefinitionBuildRequest &request)
{
    cfg.prim = request.primitive_type;
    cfg.SetGeometryVertexFormat(request.geometry_vertex_format);

    if(request.override_shader_stage_bits)
        cfg.shader_stage_flag_bit = request.shader_stage_flag_bit;

    if(request.override_rt_output)
        cfg.rt_output = request.rt_output;

    if(request.private_shader_buffer_sources && request.private_shader_buffer_source_count>0)
        cfg.SetPrivateShaderBufferSources(request.private_shader_buffer_sources,request.private_shader_buffer_source_count);
}

static ShaderProgramBuildSpec *CreateMaterialCreateInfoFromRequest(const contract::PhysicalDeviceProfileLite *profile,
                                                                   const BuiltinMaterialCreatorID mtl_id,
                                                                   const MaterialDefinition &definition,
                                                                   const MaterialDefinitionBuildRequest &request)
{
    if(definition.is_text)
    {
        Text2DMaterialCreateConfig cfg;
        ApplyBuildRequestToLegacyConfig(cfg, request);
        return CreateMaterialCreateInfo(profile, mtl_id, &cfg);
    }

    if(definition.is_2d)
    {
        const WithLocalToWorld with_l2w =
            request.recipe.local_to_world_2d ? WithLocalToWorld::With : WithLocalToWorld::Without;
        Material2DCreateConfig cfg(request.primitive_type, request.recipe.coordinate_system_2d, with_l2w);
        ApplyBuildRequestToLegacyConfig(cfg, request);
        return CreateMaterialCreateInfo(profile, mtl_id, &cfg);
    }

    const WithCamera wc = definition.with_camera ? WithCamera::With : WithCamera::Without;
    const WithLocalToWorld wl = definition.with_local_to_world ? WithLocalToWorld::With : WithLocalToWorld::Without;
    const WithSky ws = definition.with_sky ? WithSky::With : WithSky::Without;
    Material3DCreateConfig cfg(request.primitive_type, wc, wl, ws);
    ApplyBuildRequestToLegacyConfig(cfg, request);
    if(request.override_sky_ambient_model)
        cfg.sky_ambient_model = request.sky_ambient_model;
    return CreateMaterialCreateInfo(profile, mtl_id, &cfg);
}

static std::string BuildBuiltinMaterialCreatorRequestHashImpl(const BuiltinMaterialCreatorID mtl_id,
                                                              const MaterialDefinition &definition,
                                                              const MaterialDefinitionBuildRequest &request)
{
    if(definition.is_text)
    {
        Text2DMaterialCreateConfig cfg;
        ApplyBuildRequestToLegacyConfig(cfg, request);
        return std::string(GetBuiltinMaterialCreatorIDName(mtl_id)) + "?" + cfg.ToHashStdString();
    }

    if(definition.is_2d)
    {
        const WithLocalToWorld with_l2w =
            request.recipe.local_to_world_2d ? WithLocalToWorld::With : WithLocalToWorld::Without;
        Material2DCreateConfig cfg(request.primitive_type, request.recipe.coordinate_system_2d, with_l2w);
        ApplyBuildRequestToLegacyConfig(cfg, request);
        return std::string(GetBuiltinMaterialCreatorIDName(mtl_id)) + "?" + cfg.ToHashStdString();
    }

    const WithCamera wc = definition.with_camera ? WithCamera::With : WithCamera::Without;
    const WithLocalToWorld wl = definition.with_local_to_world ? WithLocalToWorld::With : WithLocalToWorld::Without;
    const WithSky ws = definition.with_sky ? WithSky::With : WithSky::Without;
    Material3DCreateConfig cfg(request.primitive_type, wc, wl, ws);
    ApplyBuildRequestToLegacyConfig(cfg, request);
    if(request.override_sky_ambient_model)
        cfg.sky_ambient_model = request.sky_ambient_model;
    return std::string(GetBuiltinMaterialCreatorIDName(mtl_id)) + "?" + cfg.ToHashStdString();
}

ShaderProgramBuildSpec *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                              const BuiltinMaterialCreatorID mtl_id,
                                              const MaterialDefinition &definition,
                                              const MaterialDefinitionBuildRequest &request)
{
    return CreateMaterialCreateInfoFromRequest(profile, mtl_id, definition, request);
}

std::string BuildBuiltinMaterialCreatorRequestHash(const BuiltinMaterialCreatorID mtl_id,
                                                 const MaterialDefinition &definition,
                                                 const MaterialDefinitionBuildRequest &request)
{
    return BuildBuiltinMaterialCreatorRequestHashImpl(mtl_id, definition, request);
}
}//namespace hgl::graph::mtl
