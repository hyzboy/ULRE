#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/ShaderBufferSource.h>
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

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format)
{
    if(!gvf)
        return fallback_format;

    const GeometryVertexAttributeFormat *attribute=gvf->Find(semantic);
    if(!attribute||attribute->format==VK_FORMAT_UNDEFINED)
        return fallback_format;

    return attribute->format;
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

bool ShouldUse2DFallbackMaterial(const MaterialDefinitionBuildRequest &request)
{
    const GeometryVertexFormat *gvf = request.geometry_vertex_format;
    if (!gvf)
        return false;

    const GeometryVertexAttributeFormat *position = gvf->Find(VertexSemantic::Position);
    if (!position)
        return false;

    return position->vec_size == 2;
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

static bool Is2DBuiltinMaterial(const BuiltinMaterialCreatorID mtl_id) noexcept
{
    return mtl_id <= BuiltinMaterialCreatorID::Text2D;
}

static ShaderProgramBuildSpec *CreateMaterialCreateInfoFromRequest(const contract::PhysicalDeviceProfileLite *profile,
                                                                   const BuiltinMaterialCreatorID mtl_id,
                                                                   const MaterialDefinition &definition,
                                                                   const MaterialDefinitionBuildRequest &request)
{
    if (Is2DBuiltinMaterial(mtl_id))
    {
        if (mtl_id == BuiltinMaterialCreatorID::Text2D)
            return CreateText2D(profile, request, definition);

        switch(mtl_id)
        {
            case BuiltinMaterialCreatorID::VertexColor2D:      return CreateVertexColor2D(profile, request, definition);
            case BuiltinMaterialCreatorID::PureColor2D:        return CreatePureColor2D(profile, request, definition);
            case BuiltinMaterialCreatorID::PureTexture2D:      return CreatePureTexture2D(profile, request, definition);
            case BuiltinMaterialCreatorID::RectTexture2D:      return CreateRectTexture2D(profile, request, definition);
            case BuiltinMaterialCreatorID::RectTexture2DArray: return CreateRectTexture2DArray(profile, request, definition);
            case BuiltinMaterialCreatorID::Text2D:             return CreateText2D(profile, request, definition);
            default:                                           break;
        }
    }

    switch(mtl_id)
    {
        case BuiltinMaterialCreatorID::PureColor3D:           return CreatePureColor3D(profile, request, definition);
        case BuiltinMaterialCreatorID::VertexColor3D:         return CreateVertexColor3D(profile, request, definition);
        case BuiltinMaterialCreatorID::VertexLuminance3D:     return CreateVertexLuminance3D(profile, request, definition);
        case BuiltinMaterialCreatorID::VertexPattleColor3D:   return CreateVertexPattleColor3D(profile, request, definition);
        case BuiltinMaterialCreatorID::Gizmo3D:               return CreateGizmo3D(profile, request, definition);
        case BuiltinMaterialCreatorID::SkyMinimal:            return CreateSkyMinimal(profile, request, definition);
        case BuiltinMaterialCreatorID::Standard:              return CreateStandard(profile, request, definition);
        case BuiltinMaterialCreatorID::StandardTextureArray:  return CreateStandardTextureArray(profile, request, definition);
        case BuiltinMaterialCreatorID::PBRColor3D:            return CreatePBRColor3D(profile, request, definition);
        default:                                              break;
    }
    return nullptr;
}

static std::string BuildBuiltinMaterialCreatorRequestHashImpl(const BuiltinMaterialCreatorID mtl_id,
                                                              const MaterialDefinition &definition,
                                                              const MaterialDefinitionBuildRequest &request)
{
    auto BuildCommonConfigHash = [](const uint32 ssbo_slot_count,
                                    const RenderTargetOutputConfig &rt_output,
                                    const uint32 shader_stage_flag_bit,
                                    const PrimitiveType prim,
                                    const GeometryVertexFormat *geometry_vertex_format,
                                    const ShaderBufferSource *const *private_shader_buffer_sources,
                                    const uint32 private_shader_buffer_source_count) -> std::string
    {
        std::string hash;
        hash.reserve(128);
        hash+='M';

        if(ssbo_slot_count>0)
        {
            hash+='S';
            hash+=std::to_string(ssbo_slot_count);
        }

        hash+='_';
        hash+=char('0'+rt_output.color);

        if(rt_output.depth){hash+='D';}
        if(rt_output.stencil){hash+='S';}

        hash+='_';

        if(shader_stage_flag_bit&(uint32)ShaderStage::Vertex){hash+='V';}
        if(shader_stage_flag_bit&(uint32)ShaderStage::TessControl){hash+='T';}     //tc/te有一个就行了
        if(shader_stage_flag_bit&(uint32)ShaderStage::Geometry){hash+='G';}
        if(shader_stage_flag_bit&(uint32)ShaderStage::Fragment){hash+='F';}
        if(shader_stage_flag_bit&(uint32)ShaderStage::Compute){hash+='C';}
        if(shader_stage_flag_bit&(uint32)ShaderStage::Mesh){hash+='M';}     //mesh/task有一个就行了
        hash+='_';

        if(const char *prim_name=GetPrimName(prim))
            hash+=prim_name;
        else
            hash+="UnknownPrim";

        if(private_shader_buffer_source_count>0)
        {
            hash+="_PS";
            const std::string pss_count_str=std::to_string(private_shader_buffer_source_count);
            hash+=pss_count_str;

            for(uint32 i=0;i<private_shader_buffer_source_count;++i)
            {
                hash+="_";

                const ShaderBufferSource *sbs=private_shader_buffer_sources?private_shader_buffer_sources[i]:nullptr;
                if(sbs&&sbs->struct_name)
                    hash+=sbs->struct_name;
                else
                    hash+="null";
            }
        }

        if(geometry_vertex_format&&geometry_vertex_format->GetCount()>0)
        {
            hash+="_GVF";

            for(uint32 i=0;i<geometry_vertex_format->GetCount();++i)
            {
                const GeometryVertexAttributeFormat *attribute=geometry_vertex_format->Get(i);
                if(!attribute)
                    continue;

                hash+="_";
                hash+=GetVertexSemanticName(attribute->semantic);
                hash+="_F";
                hash+=std::to_string((uint32_t)attribute->format);
                hash+="_V";
                hash+=std::to_string((uint32_t)attribute->vec_size);
                hash+="_S";
                hash+=std::to_string(attribute->stride);
            }
        }

        return hash;
    };

    auto BuildNodeConfigHash = [&](const VertexShaderNodeConfig &cfg) -> std::string
    {
       std::string hash;
        hash.reserve(64);
        hash += "_IN";
        hash += std::to_string(static_cast<uint32>(cfg.input));
        hash += "_PM";
        hash += std::to_string(static_cast<uint32>(cfg.position_mapping));
        hash += "_OR";
        hash += std::to_string(static_cast<uint32>(cfg.orientation));
        hash += "_SC";
        hash += std::to_string(static_cast<uint32>(cfg.scale));
        hash += "_PR";
        hash += std::to_string(static_cast<uint32>(cfg.projection));
        return hash;
    };

    auto Build3DConfigHash = [&]() -> std::string
    {
        RenderTargetOutputConfig rt_output{};
        rt_output.color = 1;
        rt_output.depth = true;
        rt_output.stencil = false;
        if(request.override_rt_output)
            rt_output = request.rt_output;

        uint32 shader_stage_flag_bit = uint32(ShaderStage::VertexFragment);
        if(request.override_shader_stage_bits)
            shader_stage_flag_bit = request.shader_stage_flag_bit;

        const ShaderBufferSource *const *private_sbs = nullptr;
        uint32 private_sbs_count = 0;
        if(request.private_shader_buffer_sources && request.private_shader_buffer_source_count>0)
        {
            private_sbs = request.private_shader_buffer_sources;
            private_sbs_count = request.private_shader_buffer_source_count;
        }

        std::string hash = BuildCommonConfigHash(static_cast<uint32>(definition.ssbo_slot_decls.size()),
                                                 rt_output,
                                                 shader_stage_flag_bit,
                                                 request.primitive_type,
                                                 request.geometry_vertex_format,
                                                 private_sbs,
                                                 private_sbs_count);

        if (HasUBORequirement(definition, UBODescriptorSemantic::CameraInfo))
            hash+="_Camera";

        if (HasUBORequirement(definition, UBODescriptorSemantic::SkyInfo))
            hash+="_Sky";

        hash+="_Amb";
        const SkyLightAmbientModel ambient = request.override_sky_ambient_model
                                           ? request.sky_ambient_model
                                           : SkyLightAmbientModel::Simple;
        char amb_model_str[2]={(char)('0'+(uint8)ambient),0};
        hash+=amb_model_str;

        if (definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
         && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough)
            hash+="_L2W";

        hash += BuildNodeConfigHash(definition.vertex_node_config);

        return hash;
    };

    const char *creator_name = GetBuiltinMaterialCreatorIDName(mtl_id);
    if(!creator_name || !*creator_name)
        creator_name = "UnknownBuiltinMaterialCreatorID";

    if (Is2DBuiltinMaterial(mtl_id))
    {
    RenderTargetOutputConfig rt_output{};
    rt_output.color = 1;
    rt_output.depth = false;
    rt_output.stencil = false;

    uint32 shader_stage_flag_bit = uint32(ShaderStage::VertexFragment);
    if(request.override_shader_stage_bits)
        shader_stage_flag_bit = request.shader_stage_flag_bit;

    const ShaderBufferSource *const *private_sbs = nullptr;
    uint32 private_sbs_count = 0;
    if(request.override_rt_output)
        rt_output = request.rt_output;
    if(request.private_shader_buffer_sources && request.private_shader_buffer_source_count>0)
    {
        private_sbs = request.private_shader_buffer_sources;
        private_sbs_count = request.private_shader_buffer_source_count;
    }

    std::string hash = BuildCommonConfigHash(static_cast<uint32>(definition.ssbo_slot_decls.size()),
                                             rt_output,
                                             shader_stage_flag_bit,
                                             request.primitive_type,
                                             request.geometry_vertex_format,
                                             private_sbs,
                                             private_sbs_count);
    hash += BuildNodeConfigHash(request.recipe.vertex_node_config);
    return std::string(creator_name) + "?" + hash;
    }

    const std::string hash = Build3DConfigHash();
    return std::string(creator_name) + "?" + hash;
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

void NormalizeRecipe(MaterialRecipe &recipe)
{
    if (recipe.mtl_def_id.empty())
        return;

    MaterialDefinition bmi{};
    const bool has_definition = TryGetMaterialDefinitionByID(recipe.mtl_def_id, bmi);
    if (has_definition)
        ApplyBaseMaterialInfoDefaults(recipe, bmi, false);

}

}//namespace hgl::graph::mtl
