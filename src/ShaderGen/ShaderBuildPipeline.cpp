#include<hgl/shadergen/ShaderBuildPipeline.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/DescriptorLayoutBuilder.h>
#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/shadergen/MaterialDescriptorStageBinder.h>
#include<hgl/shadergen/MaterialBuilder.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/MaterialInstanceConfigurator.h>
#include<hgl/shadergen/ShaderLibraryPath.h>
#include<hgl/shadergen/internal/GLSLSourceUtils.h>
#include<hgl/mtl/MaterialFeature.h>
#include<hgl/mtl/UBOCommon.h>

namespace
{
static bool BuildMaterialInstanceSchemaSnippet(const hgl::graph::mtl::MaterialInstanceBlock &material_instance,
                                               std::string &snippet,
                                               hgl::graph::ShaderGenDiagnostic &diagnostic)
{
    if(material_instance.schema==hgl::graph::mtl::ShaderDataSchema::None)
        return true;

    if(material_instance.schema_file.empty())
    {
        diagnostic={hgl::graph::ShaderGenSeverity::Error,
                    hgl::graph::ShaderGenErrorCode::SourceGenerationFailed,
                    hgl::graph::ShaderStage::Vertex,
                    "ShaderBuildPipeline.MaterialInstance.Schema",
                    "material_instance schema_file is empty during source generation"};
        return false;
    }

    snippet += "#define ULRE_PIPELINE_MATERIAL_INSTANCE_SCHEMA ";
    snippet += material_instance.schema_file;
    snippet += "\n";
    snippet += "// material_instance schema injected for pipeline compile path\n";
    snippet += "#include \"";
    snippet += "common/schema/";
    snippet += material_instance.schema_file;
    snippet += "\"\n";
    return true;
}

static bool ResolveConfiguredCameraRequirement(const hgl::graph::mtl::Material3DCreateConfig &cfg)
{
    if(cfg.effective_feature_mask!=0)
        return hgl::graph::mtl::HasFeature(cfg.effective_feature_mask,hgl::graph::mtl::MaterialFeature::NeedsCamera);

    return cfg.camera;
}

static bool ResolveConfiguredSkyRequirement(const hgl::graph::mtl::Material3DCreateConfig &cfg)
{
    if(cfg.effective_feature_mask!=0)
        return hgl::graph::mtl::HasFeature(cfg.effective_feature_mask,hgl::graph::mtl::MaterialFeature::NeedsSky);

    return cfg.sky;
}

static void AppendDiagnosticLine(std::string *diagnostics,const std::string &line)
{
    if(!diagnostics||line.empty())
        return;

    if(!diagnostics->empty())
        *diagnostics += '\n';

    *diagnostics += line;
}

static void EmitInferenceMismatchDiagnostics(const hgl::graph::mtl::StaticMaterialDef &def,
                                            const hgl::graph::mtl::Material3DCreateConfig &cfg,
                                            const bool infer_has_camera,
                                            const bool infer_has_sky,
                                            std::string *diagnostics)
{
    const bool configured_camera=ResolveConfiguredCameraRequirement(cfg);
    const bool configured_sky=ResolveConfiguredSkyRequirement(cfg);

    auto emit_one = [&](const char *label,const bool configured,const bool inferred)
    {
        if(configured==inferred)
            return;

        std::string message;
        message.reserve(256);
        message += "[CompositorCompiler] inferred ";
        message += label;
        message += "=";
        message += inferred ? "true" : "false";
        message += " differs from configured/effective=";
        message += configured ? "true" : "false";
        message += " for material='";
        message += def.name ? def.name : "<unnamed>";
        message += "'";

        if(cfg.effective_feature_mask!=0)
        {
            char buf[96]{};
            std::snprintf(buf,
                          sizeof(buf),
                          " effective_feature_mask=0x%016llx",
                          static_cast<unsigned long long>(cfg.effective_feature_mask));
            message += buf;
        }

        message += "; compiler inference is diagnostics-only";

        std::fprintf(stderr,"%s\n",message.c_str());
        AppendDiagnosticLine(diagnostics,message);
    };

    emit_one("camera",configured_camera,infer_has_camera);
    emit_one("sky",configured_sky,infer_has_sky);
}

static bool BuildVertexShaderSource(const hgl::graph::mtl::MaterialInstanceBlock &material_instance,
                                    std::string &source,
                                    hgl::graph::ShaderGenDiagnostic &diagnostic)
{
    source = "#version 450\n";

    std::string schema_snippet;
    if(!BuildMaterialInstanceSchemaSnippet(material_instance,schema_snippet,diagnostic))
        return false;

    if(!schema_snippet.empty())
        source += schema_snippet;

    source += "void main(){}\n";
    return true;
}

static bool BuildFragmentShaderSource(const hgl::graph::mtl::MaterialInstanceBlock &material_instance,
                                      std::string &source,
                                      hgl::graph::ShaderGenDiagnostic &diagnostic)
{
    source = "#version 450\n";

    std::string schema_snippet;
    if(!BuildMaterialInstanceSchemaSnippet(material_instance,schema_snippet,diagnostic))
        return false;

    if(!schema_snippet.empty())
        source += schema_snippet;

    source += "layout(location=0) out vec4 outColor;\nvoid main(){outColor=vec4(1.0);}\n";
    return true;
}

static bool HasStageBit(const uint32_t bits,const hgl::graph::ShaderStage stage)
{
    return (bits&uint32_t(stage))!=0;
}

static hgl::graph::ShaderStage ResolveTextureBindStage(const uint32_t stage_bits)
{
    if(HasStageBit(stage_bits,hgl::graph::ShaderStage::Fragment))
        return hgl::graph::ShaderStage::Fragment;

    return hgl::graph::ShaderStage::Vertex;
}

static bool ApplyTextureSamplerOverrides(const hgl::graph::mtl::MaterialCreateConfig &config,
                                         hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(config.sampler_feature_bits_override==0)
        return true;

    const hgl::graph::ShaderStage bind_stage=ResolveTextureBindStage(config.shader_stage_flag_bit);

    for(size_t i=0;i<hgl::graph::mtl::SamplerSlotCount;++i)
    {
        const uint32_t bit=(1u<<i);

        if((config.sampler_feature_bits_override&bit)==0)
            continue;

        const auto slot=static_cast<hgl::graph::mtl::SamplerSlot>(i);

        if(!hgl::graph::mtl::MaterialDescriptorStageBinder::AddTextureSampler(descriptor_db,
                                                                               bind_stage,
                                                                               hgl::graph::SamplerType::Sampler2D,
                                                                               slot,
                                                                               hgl::graph::TextureChannelHint::RGBA))
            return false;
    }

    return true;
}

static bool AddSSBOBySemantic(hgl::graph::MaterialDescriptorDB &descriptor_db,
                              const hgl::graph::mtl::SSBODescriptorSemantic semantic,
                              const uint32_t stage_bits)
{
    if(!descriptor_db.AddSSBOStruct(semantic))
        return false;

    const auto &meta=hgl::graph::mtl::GetDescriptorSemanticMeta(semantic);
    auto *ssbo=hgl::graph::mtl::CreateSSBODescriptor(semantic,stage_bits);
    if(!ssbo)
        return false;

    return descriptor_db.AddSSBO(stage_bits,meta.set_type,ssbo)!=nullptr;
}

static bool AddUBOBySemantic(hgl::graph::MaterialDescriptorDB &descriptor_db,
                             const hgl::graph::mtl::UBODescriptorSemantic semantic,
                             const uint32_t stage_bits)
{
    if(!descriptor_db.AddUBOStruct(semantic))
        return false;

    const auto &meta=hgl::graph::mtl::GetDescriptorSemanticMeta(semantic);
    auto *ubo=hgl::graph::mtl::CreateUBODescriptor(semantic,stage_bits);
    if(!ubo)
        return false;

    return descriptor_db.AddUBO(stage_bits,meta.set_type,ubo)!=nullptr;
}

static bool ApplyDescriptorSpec(const hgl::graph::ShaderBuildDescriptorSpec *descriptor_spec,
                                const uint32_t stage_bits,
                                hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(!descriptor_spec)
        return true;

    for(const auto semantic:descriptor_spec->ubos)
    {
        if(!AddUBOBySemantic(descriptor_db,semantic,stage_bits))
            return false;
    }

    for(const auto semantic:descriptor_spec->ssbos)
    {
        if(!AddSSBOBySemantic(descriptor_db,semantic,stage_bits))
            return false;
    }

    return true;
}

static bool ApplySSBOOverrides(const hgl::graph::mtl::MaterialCreateConfig &config,
                               hgl::graph::MaterialDescriptorDB &descriptor_db)
{
    if(config.local_to_world)
    {
        if(!AddSSBOBySemantic(descriptor_db,
                              hgl::graph::mtl::SSBODescriptorSemantic::TransformData,
                              config.shader_stage_flag_bit))
            return false;
    }

    return true;
}

static bool ApplyBuildModelSpec(const hgl::graph::mtl::MaterialCreateConfig &config,
                                const hgl::graph::ShaderBuildDescriptorSpec *descriptor_spec,
                                hgl::graph::MaterialDescriptorDB &descriptor_db,
                                hgl::graph::mtl::MaterialInstanceBlock &material_instance,
                                hgl::graph::mtl::LocalToWorldBlock &local_to_world,
                                hgl::graph::ShaderGenDiagnostic &diagnostic)
{
    if(config.material_instance && descriptor_spec && descriptor_spec->material_instance_bytes>0)
    {
        if(descriptor_spec->material_instance_schema!=hgl::graph::mtl::ShaderDataSchema::None)
        {
            const auto &schema_info=hgl::graph::mtl::GetShaderDataSchemaInfo(descriptor_spec->material_instance_schema);

            if(schema_info.byte_size==0)
            {
                diagnostic={hgl::graph::ShaderGenSeverity::Error,
                            hgl::graph::ShaderGenErrorCode::SourceGenerationFailed,
                            hgl::graph::ShaderStage::Vertex,
                            "ShaderBuildPipeline.MaterialInstance.Schema",
                            "material_instance schema info has zero byte size"};
                return false;
            }

            if(!schema_info.glsl_schema_file||!*schema_info.glsl_schema_file)
            {
                diagnostic={hgl::graph::ShaderGenSeverity::Error,
                            hgl::graph::ShaderGenErrorCode::SourceGenerationFailed,
                            hgl::graph::ShaderStage::Vertex,
                            "ShaderBuildPipeline.MaterialInstance.Schema",
                            "material_instance schema file is missing"};
                return false;
            }

            if(schema_info.byte_size!=descriptor_spec->material_instance_bytes)
            {
                diagnostic={hgl::graph::ShaderGenSeverity::Error,
                            hgl::graph::ShaderGenErrorCode::InvalidConfig,
                            hgl::graph::ShaderStage::Vertex,
                            "ShaderBuildPipeline.MaterialInstance.Schema",
                            "material_instance schema byte size mismatch"};
                return false;
            }

            if(!hgl::graph::mtl::MaterialInstanceConfigurator::ConfigureMaterialInstance(descriptor_db,
                                                                                          material_instance,
                                                                                          0,
                                                                                          descriptor_spec->material_instance_schema,
                                                                                          schema_info,
                                                                                          config.shader_stage_flag_bit))
            {
                diagnostic={hgl::graph::ShaderGenSeverity::Error,
                            hgl::graph::ShaderGenErrorCode::InvalidConfig,
                            hgl::graph::ShaderStage::Vertex,
                            "ShaderBuildPipeline.MaterialInstance.Schema",
                            "failed to configure material_instance from schema"};
                return false;
            }
        }
        else
        {
            if(!hgl::graph::mtl::MaterialInstanceConfigurator::ConfigureMaterialInstance(descriptor_db,
                                                                                          material_instance,
                                                                                          0,
                                                                                          descriptor_spec->material_instance_bytes,
                                                                                          config.shader_stage_flag_bit))
            {
                diagnostic={hgl::graph::ShaderGenSeverity::Error,
                            hgl::graph::ShaderGenErrorCode::InvalidConfig,
                            hgl::graph::ShaderStage::Vertex,
                            "ShaderBuildPipeline.MaterialInstance",
                            "failed to configure material_instance from byte size"};
                return false;
            }
        }
    }

    if(config.local_to_world)
    {
        local_to_world.stage_bits=config.shader_stage_flag_bit;
        local_to_world.enabled=true;
    }

    return true;
}

static bool HasSSBOSemantic(const hgl::graph::mtl::StaticMaterialDef &def,
                            const hgl::graph::mtl::SSBODescriptorSemantic semantic)
{
    if(!def.ssbo_descriptors || semantic==hgl::graph::mtl::SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

static bool HasUBOSemantic(const hgl::graph::mtl::StaticMaterialDef &def,
                           const hgl::graph::mtl::UBODescriptorSemantic semantic)
{
    if(!def.ubo_descriptors || semantic==hgl::graph::mtl::UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

static bool HasPerMaterialDescriptor(const hgl::graph::mtl::StaticMaterialDef &def)
{
    if(def.ubo_descriptors)
    {
        for(const auto semantic:*def.ubo_descriptors)
        {
            if(hgl::graph::mtl::GetDescriptorSemanticMeta(semantic).set_type==hgl::graph::SET_TYPE_MATERIAL)
                return true;
        }
    }

    if(def.ssbo_descriptors)
    {
        for(const auto semantic:*def.ssbo_descriptors)
        {
            if(hgl::graph::mtl::GetDescriptorSemanticMeta(semantic).set_type==hgl::graph::SET_TYPE_MATERIAL)
                return true;
        }
    }

    if(def.texture_samplers && !def.texture_samplers->empty())
        return true;

    return false;
}

static std::string BuildSchemaIncludeText(const hgl::graph::mtl::ShaderDataSchema schema)
{
    if(schema==hgl::graph::mtl::ShaderDataSchema::None)
        return std::string();

    const auto &schema_info=hgl::graph::mtl::GetShaderDataSchemaInfo(schema);
    if(!schema_info.glsl_schema_file || !*schema_info.glsl_schema_file)
        return std::string();

    std::string include_text;
    include_text.reserve(48 + std::char_traits<char>::length(schema_info.glsl_schema_file));
    include_text += "#include \"common/schema/";
    include_text += schema_info.glsl_schema_file;
    include_text += "\"\n";
    return include_text;
}

static void AppendBuildMaterialCreateInfoDiagnostic(
    std::vector<hgl::graph::ShaderGenDiagnostic> &diagnostics,
    const hgl::graph::ShaderGenErrorCode error_code,
    const hgl::graph::ShaderStage stage,
    const char *subject,
    const char *message)
{
    diagnostics.push_back({hgl::graph::ShaderGenSeverity::Error,
                           error_code,
                           stage,
                           subject ? subject : "ShaderBuildPipeline.BuildMaterialCreateInfo",
                           message ? message : "<unknown>"});
}

static bool ApplyStaticMaterialDefToBuilder(
    const hgl::graph::mtl::StaticMaterialDef &def,
    hgl::graph::mtl::MaterialBuilder &builder,
    std::vector<hgl::graph::ShaderGenDiagnostic> &diagnostics)
{
    constexpr uint32_t kBuilderStageBits = uint32_t(hgl::graph::ShaderStage::VertexFragment);
    uint32_t mi_stage_bits = uint32_t(hgl::graph::ShaderStage::Fragment);

    const auto vertex_view = hgl::graph::mtl::BuildVertexDefFromStaticMaterialDef(def);
    const auto fragment_view = hgl::graph::mtl::BuildFragmentDefFromStaticMaterialDef(def);

    if(fragment_view.ubo_descriptors)
    {
        for(const auto semantic:*fragment_view.ubo_descriptors)
        {
            if(!builder.AddUBOStruct(kBuilderStageBits, semantic))
            {
                AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                        hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                        hgl::graph::ShaderStage::Vertex,
                                                        "ShaderBuildPipeline.BuildMaterialCreateInfo.UBO",
                                                        "failed to add UBO semantic");
                return false;
            }
        }
    }

    if(fragment_view.ssbo_descriptors)
    {
        for(const auto semantic:*fragment_view.ssbo_descriptors)
        {
            if(semantic == hgl::graph::mtl::SSBODescriptorSemantic::TransformData)
            {
                if(!builder.SetLocalToWorld(kBuilderStageBits))
                {
                    AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                            hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                            hgl::graph::ShaderStage::Vertex,
                                                            "ShaderBuildPipeline.BuildMaterialCreateInfo.LocalToWorld",
                                                            "failed to configure local_to_world");
                    return false;
                }

                continue;
            }

            if(semantic == hgl::graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData)
            {
                mi_stage_bits = kBuilderStageBits;
                continue;
            }

            if(!builder.AddSSBOStruct(kBuilderStageBits, semantic))
            {
                AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                        hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                        hgl::graph::ShaderStage::Vertex,
                                                        "ShaderBuildPipeline.BuildMaterialCreateInfo.SSBO",
                                                        "failed to add SSBO semantic");
                return false;
            }
        }
    }

    if(fragment_view.texture_samplers)
    {
        for(const auto &[slot, descriptor] : *fragment_view.texture_samplers)
        {
            if(!builder.AddTextureSampler(kBuilderStageBits,
                                          descriptor.sampler_type,
                                          slot,
                                          descriptor.channel_hint))
            {
                AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                        hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                        hgl::graph::ShaderStage::Fragment,
                                                        "ShaderBuildPipeline.BuildMaterialCreateInfo.TextureSampler",
                                                        "failed to add texture sampler");
                return false;
            }
        }
    }

    if(vertex_view.vertex_entries)
    {
        if(auto *vsc = builder.GetVertexShader())
        {
            for(uint32_t i=0;i<vertex_view.vertex_entry_count;++i)
            {
                const auto &entry=vertex_view.vertex_entries[i];
                vsc->AddInput(entry.type, entry.attrib);
            }
        }
    }

    if(fragment_view.shader_data_schema != hgl::graph::mtl::ShaderDataSchema::None)
    {
        const auto &schema_info = hgl::graph::mtl::GetShaderDataSchemaInfo(fragment_view.shader_data_schema);

        if(schema_info.byte_size==0 || !schema_info.glsl_schema_file || !*schema_info.glsl_schema_file)
        {
            AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                    hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                    hgl::graph::ShaderStage::Vertex,
                                                    "ShaderBuildPipeline.BuildMaterialCreateInfo.MaterialInstance.Schema",
                                                    "shader data schema info is incomplete");
            return false;
        }

        if(!builder.SetMaterialInstance(fragment_view.shader_data_schema, schema_info, mi_stage_bits))
        {
            AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                    hgl::graph::ShaderGenErrorCode::InvalidConfig,
                                                    hgl::graph::ShaderStage::Fragment,
                                                    "ShaderBuildPipeline.BuildMaterialCreateInfo.MaterialInstance.Schema",
                                                    "failed to configure material_instance from schema");
            return false;
        }
    }

    return true;
}

static bool ApplyFinalGLSLToBuilder(
    const hgl::graph::mtl::StaticMaterialDef &def,
    hgl::graph::mtl::MaterialBuilder &builder,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::vector<hgl::graph::ShaderGenDiagnostic> &diagnostics)
{
    std::string final_vs_glsl = vs_glsl;
    std::string final_fs_glsl = fs_glsl;

    const auto fragment_view = hgl::graph::mtl::BuildFragmentDefFromStaticMaterialDef(def);

    if(fragment_view.shader_data_schema != hgl::graph::mtl::ShaderDataSchema::None)
    {
        const std::string schema_include = BuildSchemaIncludeText(fragment_view.shader_data_schema);
        if(schema_include.empty())
        {
            AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                    hgl::graph::ShaderGenErrorCode::SourceGenerationFailed,
                                                    hgl::graph::ShaderStage::Vertex,
                                                    "ShaderBuildPipeline.BuildMaterialCreateInfo.MaterialInstance.Schema",
                                                    "shader data schema has no GLSL include path");
            return false;
        }

        final_vs_glsl = hgl::graph::internal::InjectAfterVersion(final_vs_glsl, schema_include);
        final_fs_glsl = hgl::graph::internal::InjectAfterVersion(final_fs_glsl, schema_include);
    }

    if(auto *vert = builder.GetVertexShader())
        vert->SetFinalGLSL(final_vs_glsl);

    if(auto *frag = builder.GetStageShader(hgl::graph::ShaderStage::Fragment))
        frag->SetFinalGLSL(final_fs_glsl);

    return true;
}

static hgl::graph::mtl::MaterialCreateInfo *BuildCompiledMaterialCreateInfo(
    hgl::graph::mtl::MaterialBuilder &builder,
    std::vector<hgl::graph::ShaderGenDiagnostic> &diagnostics)
{
    hgl::graph::mtl::MaterialCreateInfo *mci = builder.BuildSnapshotOnly();
    if(!mci)
    {
        AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                hgl::graph::ShaderGenErrorCode::InternalError,
                                                hgl::graph::ShaderStage::Vertex,
                                                "ShaderBuildPipeline.BuildMaterialCreateInfo",
                                                "MaterialBuilder::BuildSnapshotOnly() failed");
        return nullptr;
    }

    if(!hgl::graph::mtl::InjectLayoutDefines(*mci))
    {
        delete mci;
        AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                hgl::graph::ShaderGenErrorCode::LayoutNotFinalized,
                                                hgl::graph::ShaderStage::Vertex,
                                                "ShaderBuildPipeline.BuildMaterialCreateInfo",
                                                "InjectLayoutDefines() failed");
        return nullptr;
    }

    if(!mci->CompileShaderStagesToSPV())
    {
        delete mci;
        AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                hgl::graph::ShaderGenErrorCode::CompileFailed,
                                                hgl::graph::ShaderStage::Vertex,
                                                "ShaderBuildPipeline.BuildMaterialCreateInfo",
                                                "CompileShaderStagesToSPV() failed");
        return nullptr;
    }

    return mci;
}

static hgl::graph::mtl::MaterialCreateInfo *BuildPreparedMaterialCreateInfo(
    hgl::graph::mtl::MaterialBuilder &builder,
    std::vector<hgl::graph::ShaderGenDiagnostic> &diagnostics)
{
    hgl::graph::mtl::MaterialCreateInfo *mci = builder.BuildSnapshotOnly();
    if(!mci)
    {
        AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                hgl::graph::ShaderGenErrorCode::InternalError,
                                                hgl::graph::ShaderStage::Vertex,
                                                "ShaderBuildPipeline.PrepareMaterialCreateInfo",
                                                "MaterialBuilder::BuildSnapshotOnly() failed");
        return nullptr;
    }

    if(!hgl::graph::mtl::InjectLayoutDefines(*mci))
    {
        delete mci;
        AppendBuildMaterialCreateInfoDiagnostic(diagnostics,
                                                hgl::graph::ShaderGenErrorCode::LayoutNotFinalized,
                                                hgl::graph::ShaderStage::Vertex,
                                                "ShaderBuildPipeline.PrepareMaterialCreateInfo",
                                                "InjectLayoutDefines() failed");
        return nullptr;
    }

    return mci;
}
}

namespace hgl::graph
{
mtl::MaterialCreateConfig ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(
    const mtl::StaticMaterialDef &def,
    const mtl::MaterialCreateConfig *config)
{
    mtl::MaterialCreateConfig build_cfg = config ? *config : mtl::MaterialCreateConfig(def.primitive_type,false);

    build_cfg.prim = def.primitive_type;

    if(build_cfg.shader_stage_flag_bit == 0)
        build_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    if(def.texture_samplers)
    {
        for(const auto &[slot,descriptor]:*def.texture_samplers)
        {
            build_cfg.SetTextureSourceSlotEnabledOverride(slot,true);
            (void)descriptor;
        }
    }

    build_cfg.local_to_world = build_cfg.local_to_world
                            || HasSSBOSemantic(def, mtl::SSBODescriptorSemantic::TransformData);
    build_cfg.material_instance = build_cfg.material_instance
                               || HasSSBOSemantic(def, mtl::SSBODescriptorSemantic::MaterialBindingInstanceData)
                               || HasPerMaterialDescriptor(def)
                               || (def.shader_data_schema != mtl::ShaderDataSchema::None);

    return build_cfg;
}

mtl::Material3DCreateConfig ShaderBuildPipeline::Build3DConfigFromStaticMaterialDef(
    const mtl::StaticMaterialDef &def,
    const mtl::Material3DCreateConfig *config)
{
    mtl::Material3DCreateConfig build_cfg=config?*config:mtl::Material3DCreateConfig();
    const mtl::MaterialCreateConfig shared_cfg=BuildConfigFromStaticMaterialDef(def,
                                                                                static_cast<const mtl::MaterialCreateConfig *>(config));

    static_cast<mtl::MaterialCreateConfig &>(build_cfg)=shared_cfg;
    return build_cfg;
}

std::string ShaderBuildPipeline::BuildShaderDataSchemaDebugText(
    const mtl::StaticMaterialDef &def)
{
    if(def.shader_data_schema==mtl::ShaderDataSchema::None)
        return std::string("schema=<none>");

    const auto &schema_info=mtl::GetShaderDataSchemaInfo(def.shader_data_schema);

    std::string text;
    text.reserve(128);
    text += "schema=";
    text += std::to_string(static_cast<uint32_t>(def.shader_data_schema));
    text += " file=";
    text += schema_info.glsl_schema_file ? schema_info.glsl_schema_file : "<null>";
    text += " bytes=";
    text += std::to_string(schema_info.byte_size);
    return text;
}

ShaderBuildDescriptorSpec ShaderBuildPipeline::BuildDescriptorSpecFromStaticMaterialDef(
    const mtl::StaticMaterialDef &def)
{
    ShaderBuildDescriptorSpec spec{};

    if(def.ubo_descriptors)
    {
        for(const auto semantic:*def.ubo_descriptors)
            spec.ubos.push_back(semantic);
    }

    if(def.ssbo_descriptors)
    {
        for(const auto semantic:*def.ssbo_descriptors)
        {
            if(semantic==mtl::SSBODescriptorSemantic::TransformData)
                continue;

            spec.ssbos.push_back(semantic);
        }
    }

    if(def.shader_data_schema!=mtl::ShaderDataSchema::None)
    {
        const auto &schema_info=mtl::GetShaderDataSchemaInfo(def.shader_data_schema);
        spec.material_instance_schema=def.shader_data_schema;
        spec.material_instance_bytes=schema_info.byte_size;
    }

    return spec;
}

ShaderGenResult<ShaderBuildResult> ShaderBuildPipeline::Build(const mtl::MaterialCreateConfig &config,
                                                              const mtl::contract::PhysicalDeviceProfileLite *profile,
                                                              const ShaderBuildDescriptorSpec *descriptor_spec)
{
    ShaderGenResult<ShaderBuildResult> result{};

    ShaderBuildState state=ShaderBuildState::Empty;

    if(config.shader_stage_flag_bit==0)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "shader_stage_flag_bit is zero"});
        return result;
    }

    state=ShaderBuildState::ConfigValidated;

    if(!profile)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "physical device profile is null"});
        return result;
    }

    if(config.material_instance && (!descriptor_spec || descriptor_spec->material_instance_bytes==0))
    {
        const bool has_schema=descriptor_spec&&descriptor_spec->material_instance_schema!=mtl::ShaderDataSchema::None;
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      has_schema?"ShaderBuildPipeline.MaterialInstance.Schema":"ShaderBuildPipeline.SSBO.MaterialInstance",
                                      has_schema?"material_instance schema requires non-zero byte size":"material_instance descriptor path is not aligned yet"});
        return result;
    }

    if(config.texture_source_bits_override!=0 && config.sampler_feature_bits_override==0)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Fragment,
                                      "ShaderBuildPipeline.TextureSampler",
                                      "texture source override requires sampler slot override"});
        return result;
    }

    MaterialDescriptorDB descriptor_db;
    mtl::DescriptorBindingSlots binding_contract{};
    ShaderGenDiagnostic build_model_diagnostic{};

    if(!ApplyBuildModelSpec(config,
                            descriptor_spec,
                            descriptor_db,
                            result.value.material_instance,
                            result.value.local_to_world,
                            build_model_diagnostic))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back(build_model_diagnostic.subject.empty()
                                         ? ShaderGenDiagnostic{ShaderGenSeverity::Error,
                                                               ShaderGenErrorCode::InvalidConfig,
                                                               ShaderStage::Vertex,
                                                               "ShaderBuildPipeline.BuildModel",
                                                               "failed to apply build model spec"}
                                         : build_model_diagnostic);
        return result;
    }

    if(!ApplyDescriptorSpec(descriptor_spec,config.shader_stage_flag_bit,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.DescriptorSpec",
                                      "failed to apply descriptor spec"});
        return result;
    }

    if(!ApplySSBOOverrides(config,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.SSBO",
                                      "failed to apply SSBO overrides"});
        return result;
    }

    if(!ApplyTextureSamplerOverrides(config,descriptor_db))
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Fragment,
                                      "ShaderBuildPipeline.TextureSampler",
                                      "failed to apply texture sampler overrides"});
        return result;
    }

    mtl::DescriptorLayoutBuilder::Finalize(descriptor_db,binding_contract);

    result.value.binding_contract=binding_contract;
    result.value.descriptor_count=descriptor_db.GetCount();
    result.value.layout_finalized=true;

    state=ShaderBuildState::DescriptorLayoutFinalized;

    ShaderCompileRequest request{};
    request.profile=*profile;
    request.vulkan_version=0;
    request.spv_version=0;

    ShaderGenDiagnostic source_diagnostic{};

    if(config.shader_stage_flag_bit&uint32_t(ShaderStage::Vertex))
    {
        request.stage=ShaderStage::Vertex;
        if(!BuildVertexShaderSource(result.value.material_instance,request.source,source_diagnostic))
        {
            result.success=false;
            result.value.final_state=ShaderBuildState::Failed;
            result.diagnostics.push_back(source_diagnostic);
            return result;
        }
    }
    else
    if(config.shader_stage_flag_bit&uint32_t(ShaderStage::Fragment))
    {
        request.stage=ShaderStage::Fragment;
        if(!BuildFragmentShaderSource(result.value.material_instance,request.source,source_diagnostic))
        {
            result.success=false;
            result.value.final_state=ShaderBuildState::Failed;
            result.diagnostics.push_back(source_diagnostic);
            return result;
        }
    }

    if(request.source.empty())
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidShaderStage,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline",
                                      "no supported stage found for minimal pipeline"});
        return result;
    }

    state=ShaderBuildState::SourceGenerated;

    if(result.value.material_instance.schema!=mtl::ShaderDataSchema::None)
    {
        result.diagnostics.push_back({ShaderGenSeverity::Info,
                                      ShaderGenErrorCode::None,
                                      request.stage,
                                      "ShaderBuildPipeline.MaterialInstance.Schema",
                                      std::string("schema-aware compile path active: ")+result.value.material_instance.schema_file});
    }

    ShaderCompilerContext compiler(*profile);
    ShaderGenResult<ShaderBinary> compile_result=compiler.Compile(request);

    if(!compile_result.success)
    {
        result.success=false;
        result.value.final_state=ShaderBuildState::Failed;
        result.diagnostics.insert(result.diagnostics.end(),compile_result.diagnostics.begin(),compile_result.diagnostics.end());
        return result;
    }

    state=ShaderBuildState::Compiled;

    result.value.final_state=state;
    result.value.binaries.push_back(std::move(compile_result.value));
    result.success=true;
    return result;
}

ShaderGenResult<mtl::MaterialCreateInfo *> ShaderBuildPipeline::PrepareMaterialCreateInfo(
    const mtl::StaticMaterialDef &def,
    const mtl::MaterialCreateConfig &config,
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const std::string &vs_glsl,
    const std::string &fs_glsl)
{
    ShaderGenResult<mtl::MaterialCreateInfo *> result{};
    result.value=nullptr;

    if(vs_glsl.empty()||fs_glsl.empty())
    {
        result.success=false;
        AppendBuildMaterialCreateInfoDiagnostic(result.diagnostics,
                                                ShaderGenErrorCode::InvalidConfig,
                                                ShaderStage::Vertex,
                                                "ShaderBuildPipeline.PrepareMaterialCreateInfo",
                                                "vs_glsl or fs_glsl is empty");
        return result;
    }

    mtl::MaterialBuilder builder(&config);
    if(profile)
        builder.SetDevice(profile);

    if(!ApplyStaticMaterialDefToBuilder(def,builder,result.diagnostics))
    {
        result.success=false;
        return result;
    }

    if(!ApplyFinalGLSLToBuilder(def,builder,vs_glsl,fs_glsl,result.diagnostics))
    {
        result.success=false;
        return result;
    }

    result.value=BuildPreparedMaterialCreateInfo(builder,result.diagnostics);
    result.success=result.value!=nullptr;
    return result;
}

ShaderGenResult<mtl::MaterialCreateInfo *> ShaderBuildPipeline::PrepareMaterialCreateInfo(
    const mtl::StaticMaterialDef &def,
    const mtl::Material3DCreateConfig *config,
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string *diagnostics)
{
    if(diagnostics)
        diagnostics->clear();

    mtl::Material3DCreateConfig cfg=Build3DConfigFromStaticMaterialDef(def,config);

    const bool infer_has_camera=HasUBOSemantic(def,mtl::UBODescriptorSemantic::CameraInfo);
    const bool infer_has_sky=HasUBOSemantic(def,mtl::UBODescriptorSemantic::SkyInfo);
    EmitInferenceMismatchDiagnostics(def,cfg,infer_has_camera,infer_has_sky,diagnostics);

    ShaderGenResult<mtl::MaterialCreateInfo *> result=PrepareMaterialCreateInfo(def,
                                                                                static_cast<const mtl::MaterialCreateConfig &>(cfg),
                                                                                profile,
                                                                                vs_glsl,
                                                                                fs_glsl);

    if(!result.success)
    {
        std::string message;
        if(!result.diagnostics.empty())
            message=result.diagnostics.front().message;
        else
            message="<unknown>";

        message += " (";
        message += BuildShaderDataSchemaDebugText(def);
        message += ")";
        AppendDiagnosticLine(diagnostics,message);
    }

    return result;
}

ShaderGenResult<mtl::MaterialCreateInfo *> ShaderBuildPipeline::BuildMaterialCreateInfo(
    const mtl::StaticMaterialDef &def,
    const mtl::MaterialCreateConfig &config,
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const std::string &vs_glsl,
    const std::string &fs_glsl)
{
    ShaderGenResult<mtl::MaterialCreateInfo *> result{};

    if(!profile)
    {
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.BuildMaterialCreateInfo",
                                      "physical device profile is null"});
        return result;
    }

    if(vs_glsl.empty() || fs_glsl.empty())
    {
        result.diagnostics.push_back({ShaderGenSeverity::Error,
                                      ShaderGenErrorCode::InvalidConfig,
                                      ShaderStage::Vertex,
                                      "ShaderBuildPipeline.BuildMaterialCreateInfo",
                                      "vs_glsl or fs_glsl is empty"});
        return result;
    }

    mtl::MaterialCreateConfig build_cfg = BuildConfigFromStaticMaterialDef(def,&config);

    mtl::MaterialBuilder builder(&build_cfg);
    builder.SetDevice(profile);

    if(!ApplyStaticMaterialDefToBuilder(def,builder,result.diagnostics))
        return result;

    if(!ApplyFinalGLSLToBuilder(def,builder,vs_glsl,fs_glsl,result.diagnostics))
        return result;

    mtl::MaterialCreateInfo *mci = BuildCompiledMaterialCreateInfo(builder,result.diagnostics);
    if(!mci)
        return result;

    result.success = true;
    result.value = mci;
    return result;
}

ShaderGenResult<mtl::MaterialCreateInfo *> ShaderBuildPipeline::BuildProduct(
    const mtl::StaticMaterialDef &def,
    const mtl::MaterialCreateConfig &config,
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const std::string &vs_glsl,
    const std::string &fs_glsl)
{
    return BuildMaterialCreateInfo(def,config,profile,vs_glsl,fs_glsl);
}
}//namespace hgl::graph
