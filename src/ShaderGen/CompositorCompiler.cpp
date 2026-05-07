/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl {

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic);
static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic);
static bool HasPerMaterialDescriptor(const StaticMaterialDef &def);

namespace
{
    static constexpr uint32_t kDefaultDescriptorStageBits = uint32_t(ShaderStage::VertexFragment);

    static bool ResolveConfiguredCameraRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsCamera);

        return cfg.camera;
    }

    static bool ResolveConfiguredSkyRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsSky);

        return cfg.sky;
    }

    static void AppendDiagnosticLine(std::string *diagnostics, const std::string &line)
    {
        if (!diagnostics || line.empty())
            return;

        if (!diagnostics->empty())
            *diagnostics += '\n';

        *diagnostics += line;
    }

    static void EmitInferenceMismatchDiagnostics(
        const StaticMaterialDef &def,
        const Material3DCreateConfig &cfg,
        const bool infer_has_camera,
        const bool infer_has_sky,
        std::string *diagnostics)
    {
        const bool configured_camera = ResolveConfiguredCameraRequirement(cfg);
        const bool configured_sky = ResolveConfiguredSkyRequirement(cfg);

        auto emit_one = [&](const char *label, const bool configured, const bool inferred)
        {
            if (configured == inferred)
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

            if (cfg.effective_feature_mask != 0)
            {
                char buf[96]{};
                std::snprintf(buf,
                              sizeof(buf),
                              " effective_feature_mask=0x%016llx",
                              static_cast<unsigned long long>(cfg.effective_feature_mask));
                message += buf;
            }

            message += "; compiler inference is diagnostics-only";

            std::fprintf(stderr, "%s\n", message.c_str());
            AppendDiagnosticLine(diagnostics, message);
        };

        emit_one("camera", configured_camera, infer_has_camera);
        emit_one("sky", configured_sky, infer_has_sky);
    }

    static std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def)
    {
        if (def.shader_data_schema == ShaderDataSchema::None)
            return std::string("schema=<none>");

        const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

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

    static std::string BuildShaderDataSchemaIncludeText(const ShaderDataSchemaInfo &schema_info)
    {
        if (!schema_info.glsl_schema_file || !schema_info.glsl_schema_file[0])
            return std::string();

        std::string include_text;
        include_text.reserve(48 + std::char_traits<char>::length(schema_info.glsl_schema_file));
        include_text += "#include \"common/schema/";
        include_text += schema_info.glsl_schema_file;
        include_text += "\"\n";
        return include_text;
    }

    static ShaderBuildDescriptorSpec BuildDescriptorSpecFromStaticMaterialDef(const StaticMaterialDef &def)
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
                if(semantic==SSBODescriptorSemantic::TransformData)
                    continue;

                spec.ssbos.push_back(semantic);
            }
        }

        if(def.shader_data_schema!=ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info=GetShaderDataSchemaInfo(def.shader_data_schema);
            spec.material_instance_schema=def.shader_data_schema;
            spec.material_instance_bytes=schema_info.byte_size;
        }

        return spec;
    }

    static MaterialCreateConfig BuildPipelineConfigFromCompositorConfig(const StaticMaterialDef &def,
                                                                       const Material3DCreateConfig *config)
    {
        MaterialCreateConfig pipeline_cfg(def.primitive_type,false);

        if(config)
        {
            pipeline_cfg=*static_cast<const MaterialCreateConfig *>(config);
            pipeline_cfg.prim=def.primitive_type;
        }

        pipeline_cfg.shader_stage_flag_bit=uint32_t(ShaderStage::VertexFragment);

        if(def.texture_samplers)
        {
            for(const auto &[slot,descriptor]:*def.texture_samplers)
            {
                pipeline_cfg.SetTextureSourceSlotEnabledOverride(slot,true);
                if(descriptor.sampler_type!=SamplerType::Sampler2D)
                    continue;
            }
        }

        const bool infer_has_l2w=HasSSBOSemantic(def,SSBODescriptorSemantic::TransformData);
        const bool infer_has_mi=HasSSBOSemantic(def,SSBODescriptorSemantic::MaterialBindingInstanceData)
                              || HasPerMaterialDescriptor(def)
                              || (def.shader_data_schema!=ShaderDataSchema::None);

        pipeline_cfg.local_to_world = pipeline_cfg.local_to_world || infer_has_l2w;
        pipeline_cfg.material_instance = pipeline_cfg.material_instance || infer_has_mi;
        return pipeline_cfg;
    }

    static void AppendShadowBuildDiagnostics(std::string &text,
                                             const ShaderGenResult<ShaderBuildResult> &result)
    {
        for(const auto &diag:result.diagnostics)
        {
            if(!text.empty())
                text += " | ";

            text += diag.subject;
            text += ": ";
            text += diag.message;
        }
    }

    static std::string SanitizeArtifactName(const char *text)
    {
        if(!text || !*text)
            return std::string("unnamed_material");

        std::string sanitized;
        sanitized.reserve(std::char_traits<char>::length(text));

        for(const char ch:std::string(text))
        {
            if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-')
                sanitized.push_back(ch);
            else
                sanitized.push_back('_');
        }

        return sanitized.empty()?std::string("unnamed_material"):sanitized;
    }

    static bool EnsureDirectoryExists(const char *dir)
    {
        if(!dir || !*dir)
            return false;

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(dir),ec);
        return !ec;
    }

    static bool WriteTextFile(const std::filesystem::path &path,const std::string &text)
    {
        std::ofstream ofs(path,std::ios::out|std::ios::trunc);
        if(!ofs.is_open())
            return false;

        ofs << text;
        return ofs.good();
    }

    static std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                                  const char *material_name)
    {
        std::string text;
        text.reserve(512);
        text += "material=";
        text += material_name ? material_name : "<unnamed>";
        text += "\n";
        text += "summary=";
        text += report.summary;
        text += "\n";

        for(const auto &diag:report.result.diagnostics)
        {
            text += "diag.subject=";
            text += diag.subject;
            text += ", diag.message=";
            text += diag.message;
            text += "\n";
        }

        return text;
    }

    static std::string BuildBaselineCompareReportText(const CompileCompositorShadowBuildReport &report,
                                                      const char *material_name,
                                                      const bool legacy_compile_success,
                                                      const char *legacy_summary)
    {
        std::string text;
        text.reserve(1024);
        text += "# ShaderGen 基线对比报告（自动生成）\n\n";
        text += "- Material: `";
        text += material_name ? material_name : "<unnamed>";
        text += "`\n";
        text += "- Legacy compile: `";
        text += legacy_compile_success ? "success" : "failed";
        text += "`\n";
        text += "- Pipeline shadow compile: `";
        text += report.result.success ? "success" : "failed";
        text += "`\n\n";
        text += "## Route-Switch Readiness\n\n";
        text += "- Readiness: `";
        text += report.summary;
        text += "`\n\n";
        text += "## Legacy 摘要\n\n";
        text += "- LegacySummary: `";
        text += legacy_summary ? legacy_summary : (legacy_compile_success ? "compile succeeded" : "compile failed");
        text += "`\n\n";
        text += "## Shadow Diagnostics\n\n";

        if(report.result.diagnostics.empty())
        {
            text += "- `<none>`\n";
        }
        else
        {
            for(const auto &diag:report.result.diagnostics)
            {
                text += "- `";
                text += diag.subject;
                text += "`: ";
                text += diag.message;
                text += "\n";
            }
        }

        return text;
    }

    static MaterialCreateInfo *CreatePreparedCompositorMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const StaticMaterialDef &def,
        const std::string &vs_glsl,
        const std::string &fs_glsl,
        const Material3DCreateConfig *config,
        std::string *diagnostics)
    {
        if (diagnostics)
            diagnostics->clear();

        if (vs_glsl.empty() || fs_glsl.empty())
        {
            if (diagnostics)
                *diagnostics = "vs_glsl or fs_glsl is empty";
            return nullptr;
        }

        Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
        cfg.prim = config ? config->prim : def.primitive_type;
        cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

        const bool infer_has_camera = HasUBOSemantic(def, UBODescriptorSemantic::CameraInfo);
        const bool infer_has_sky    = HasUBOSemantic(def, UBODescriptorSemantic::SkyInfo);
        const bool infer_has_l2w    = HasSSBOSemantic(def, SSBODescriptorSemantic::TransformData);
        const bool infer_has_mi     = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialBindingInstanceData)
                                   || HasPerMaterialDescriptor(def)
                                   || (def.shader_data_schema != ShaderDataSchema::None);

        cfg.local_to_world    = cfg.local_to_world    || infer_has_l2w;
        cfg.material_instance = cfg.material_instance || infer_has_mi;

        EmitInferenceMismatchDiagnostics(def,
                         cfg,
                         infer_has_camera,
                         infer_has_sky,
                         diagnostics);

        MaterialBuilder builder(&cfg);
        if (profile)
            builder.SetDevice(profile);

        auto FailWithBuilder = [&](const char *reason) -> MaterialCreateInfo *
        {
            if (diagnostics)
            {
                *diagnostics = reason ? reason : "<unknown>";
                *diagnostics += " (";
                *diagnostics += BuildShaderDataSchemaDebugText(def);
                *diagnostics += ")";
            }
            return nullptr;
        };

        uint32_t mi_stage_bits = uint32_t(ShaderStage::Fragment);

        if (def.ubo_descriptors)
        {
            for (const auto semantic : *def.ubo_descriptors)
            {
                if (!builder.AddUBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddUBO() failed");
            }
        }

        if (def.ssbo_descriptors)
        {
            for (const auto semantic : *def.ssbo_descriptors)
            {
                if (semantic == SSBODescriptorSemantic::TransformData)
                {
                    builder.SetLocalToWorld(kDefaultDescriptorStageBits);
                    continue;
                }

                if (semantic == SSBODescriptorSemantic::MaterialBindingInstanceData)
                {
                    mi_stage_bits = kDefaultDescriptorStageBits;
                    continue;
                }

                if (!builder.AddSSBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddSSBO() failed");
            }
        }

        if (def.texture_samplers)
        {
            for (const auto &[slot, descriptor] : *def.texture_samplers)
            {
                if (!RangeCheck(descriptor.sampler_type))
                    return FailWithBuilder("texture sampler slot has invalid SamplerType");

                // Use default descriptor stage bits for texture samplers
                if (!builder.AddTextureSampler(kDefaultDescriptorStageBits,
                                            descriptor.sampler_type,
                                            slot,
                                            descriptor.channel_hint))
                {
                    return FailWithBuilder("AddTextureSampler(slot) failed");
                }
            }
        }

        ShaderCreateInfoVertex *vsc = builder.GetVertexShader();
        if (vsc)
        {
            for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
            {
                const FixedVertexEntry &entry = def.vertex_entries[i];
                vsc->AddInput(entry.type, entry.attrib);
            }
        }

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

            if (schema_info.byte_size == 0)
                return FailWithBuilder("shader data schema has zero byte size");

            if (!builder.SetMaterialInstance(def.shader_data_schema, schema_info, mi_stage_bits))
                return FailWithBuilder("SetMaterialInstance() failed");
        }

        ShaderCreateInfoVertex *vert = builder.GetVertexShader();
        ShaderCreateInfo *frag = builder.GetStageShader(ShaderStage::Fragment);

        std::string final_vs_glsl = vs_glsl;
        std::string final_fs_glsl = fs_glsl;

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);
            const std::string schema_include = BuildShaderDataSchemaIncludeText(schema_info);

            if (schema_include.empty())
                return FailWithBuilder("shader data schema has no GLSL include path");

            final_vs_glsl = hgl::graph::internal::InjectAfterVersion(final_vs_glsl, schema_include);
            final_fs_glsl = hgl::graph::internal::InjectAfterVersion(final_fs_glsl, schema_include);
        }

        if (vert)
            vert->SetFinalGLSL(final_vs_glsl);

        if (frag)
            frag->SetFinalGLSL(final_fs_glsl);

        MaterialCreateInfo *mci = builder.BuildSnapshotOnly();
        if (!mci)
            return FailWithBuilder("MaterialBuilder::BuildSnapshotOnly() failed");

        if (!InjectLayoutDefines(*mci))
        {
            delete mci;
            return FailWithBuilder("InjectLayoutDefines() failed");
        }

        return mci;
    }
}

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic)
{
    if (!def.ubo_descriptors || semantic == UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic)
{
    if (!def.ssbo_descriptors || semantic == SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

static bool HasPerMaterialDescriptor(const StaticMaterialDef &def)
{
    if (def.ubo_descriptors)
    {
        for (const auto semantic : *def.ubo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto semantic : *def.ssbo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.texture_samplers)
    {
        if (!def.texture_samplers->empty())
            return true;
    }

    return false;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    const CompileCompositorRouteDecision route_decision=ResolveCompileCompositorRouteDecision(nullptr);

    std::fprintf(stderr,
        "[CompileCompositorMaterial] material=%s route_decision: %s\n",
        def.name ? def.name : "<unnamed>",
        GetCompileCompositorRouteDecisionSummary(route_decision).c_str());

    CompileCompositorShadowBuildReport shadow_report{};
    bool has_shadow_report=false;

    if(route_decision.pipeline_trial_requested)
    {
        shadow_report=BuildCompileCompositorShadowPipelineReport(profile,def,config);
        has_shadow_report=true;

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow: %s\n",
            def.name ? def.name : "<unnamed>",
            shadow_report.summary.empty() ? "<empty>" : shadow_report.summary.c_str());

        if(WriteCompileCompositorShadowBuildArtifacts(shadow_report,def.name))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts=build/shadergen_trial/reports\n",
                def.name ? def.name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts_write_failed\n",
                def.name ? def.name : "<unnamed>");
        }
    }

    std::string diagnostics;
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(profile,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               config,
                                                               &diagnostics);
    if (!mci)
    {
        if(has_shadow_report)
        {
            WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                      def.name,
                                                      false,
                                                      diagnostics.empty() ? "CreatePreparedCompositorMaterial failed" : diagnostics.c_str());
        }

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.empty() ? "<unknown>" : diagnostics.c_str());
        return nullptr;
    }

    if (!mci->CompileShaderStagesToSPV())
    {
        if(has_shadow_report)
        {
            WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                      def.name,
                                                      false,
                                                      "CompileShaderStagesToSPV() failed");
        }

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: CompileShaderStagesToSPV() failed (check GLSLCompiler log) (%s)\n",
            def.name ? def.name : "<unnamed>",
            BuildShaderDataSchemaDebugText(def).c_str());
        delete mci;
        return nullptr;
    }

    if (!diagnostics.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s diagnostics: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.c_str());
    }

    if(has_shadow_report)
    {
        if(WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                     def.name,
                                                     true,
                                                     diagnostics.empty() ? "CompileCompositorMaterial legacy compile succeeded" : diagnostics.c_str()))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report=build/shadergen_trial/reports\n",
                def.name ? def.name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report_write_failed\n",
                def.name ? def.name : "<unnamed>");
        }
    }

    return mci;
}

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics)
{
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(nullptr,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               nullptr,
                                                               diagnostics);
    if (!mci)
        return false;

    ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

    out_vs_glsl = vert ? vert->GetFinalGLSL() : std::string();
    out_fs_glsl = frag ? frag->GetFinalGLSL() : std::string();

    delete mci;
    return true;
}

CompileCompositorRoutePlan BuildCompileCompositorRoutePlan()
{
    CompileCompositorRoutePlan plan{};
    plan.preferred_route = ShaderBuildRoute::LegacyMaterialCreateInfo;
    plan.allow_pipeline_fallback = true;
    plan.can_export_readiness = true;
    plan.can_emit_baseline_artifacts = true;
    plan.rationale = "CompileCompositorMaterial holds StaticMaterialDef + GLSL + config + profile, making it the closest unified compile/model boundary for future route-switch, fallback, readiness export, and baseline artifact emission without changing the default production path yet.";
    return plan;
}

CompileCompositorRouteDecision ResolveCompileCompositorRouteDecision(const ShaderBuildSwitchConfig *switch_config)
{
    const CompileCompositorRoutePlan plan=BuildCompileCompositorRoutePlan();

    CompileCompositorRouteDecision decision{};
    decision.resolved_route=ResolveShaderBuildRoute(switch_config);
    decision.pipeline_trial_requested=(decision.resolved_route==ShaderBuildRoute::Pipeline);
    decision.fallback_to_legacy=plan.allow_pipeline_fallback;
    decision.will_use_legacy_now=true;

    if(decision.pipeline_trial_requested)
    {
        decision.rationale = "Pipeline route requested for CompileCompositorMaterial, but the current F2.10 plan keeps the production entry on Legacy while preserving fallback/readiness export preparation.";
    }
    else
    {
        decision.rationale = "CompileCompositorMaterial remains on Legacy by default in F2.10; route-switch intent is evaluated but not yet wired into the production compile branch.";
    }

    return decision;
}

std::string GetCompileCompositorRouteDecisionSummary(const CompileCompositorRouteDecision &decision)
{
    std::string text;
    text.reserve(256 + decision.rationale.size());
    text += "resolved_route=";
    text += GetShaderBuildRouteName(decision.resolved_route);
    text += ", will_use_legacy_now=";
    text += decision.will_use_legacy_now ? "true" : "false";
    text += ", pipeline_trial_requested=";
    text += decision.pipeline_trial_requested ? "true" : "false";
    text += ", fallback_to_legacy=";
    text += decision.fallback_to_legacy ? "true" : "false";

    if(!decision.rationale.empty())
    {
        text += ", rationale=";
        text += decision.rationale;
    }

    return text;
}

CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const Material3DCreateConfig *config)
{
    CompileCompositorShadowBuildReport report{};

    ShaderBuildPipeline pipeline;
    const MaterialCreateConfig pipeline_cfg=BuildPipelineConfigFromCompositorConfig(def,config);
    const ShaderBuildDescriptorSpec descriptor_spec=BuildDescriptorSpecFromStaticMaterialDef(def);

    report.result=pipeline.Build(pipeline_cfg,profile,&descriptor_spec);
    report.evaluation=EvaluateShaderBuildResultForRouteSwitch(report.result);
    report.summary=GetShaderBuildRouteEvaluationSummary(report.evaluation);

    if(!report.result.success)
    {
        if(!report.summary.empty())
            report.summary += ", diagnostics=";

        AppendShadowBuildDiagnostics(report.summary,report.result);
    }

    return report;
}

bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir)
{
    if(!EnsureDirectoryExists(reports_dir))
        return false;

    const std::filesystem::path reports_path(reports_dir);
    const std::string sanitized_name=SanitizeArtifactName(material_name);

    const std::filesystem::path readiness_path=reports_path/(sanitized_name + "_readiness.txt");
    if(!WriteShaderBuildRouteEvaluationSummary(report.evaluation,readiness_path.string().c_str()))
        return false;

    const std::filesystem::path diagnostics_path=reports_path/(sanitized_name + "_diagnostics.log");
    return WriteTextFile(diagnostics_path,BuildShadowDiagnosticsText(report,material_name));
}

bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               const bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root)
{
    if(!EnsureDirectoryExists(trial_root))
        return false;

    const std::filesystem::path trial_root_path(trial_root);
    const std::filesystem::path reports_path=trial_root_path/"reports";

    if(!EnsureDirectoryExists(reports_path.string().c_str()))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path report_path=reports_path/(sanitized_name + "_baseline_compare.md");

    return WriteTextFile(report_path,
                         BuildBaselineCompareReportText(report,
                                                        material_name,
                                                        legacy_compile_success,
                                                        legacy_summary));
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config)
{
    Material3DCreateConfig cfg3d(
        config ? config->prim : def.primitive_type,
        IncludeCamera::Without,
        config && config->local_to_world ? IncludeL2W::With : IncludeL2W::Without,
        IncludeSky::Without);

    if (config)
    {
        cfg3d.rt_output                         = config->rt_output;
        cfg3d.material_instance                 = config->material_instance;
        cfg3d.shader_stage_flag_bit             = config->shader_stage_flag_bit;
    }

    return CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

bool InjectLayoutDefines(MaterialCreateInfo &mci)
{
    ShaderCreateInfoVertex *vert = mci.GetVertexShader();
    ShaderCreateInfo       *frag = mci.GetStageShader(ShaderStage::Fragment);

    mci.Resort();
    const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(mci);
    const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
    const MaterialDescriptorDB &mdi = mci.GetDescriptorInfo();
    const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Vertex)   : std::string();
    const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Fragment) : std::string();
    const std::string frag_mit_defs     = frag ? hgl::graph::EmitMaterialInstanceTextureGLSL(mdi, ShaderStage::Fragment) : std::string();

    if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty() || !frag_mit_defs.empty())
    {
        if (vert) vert->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(vert->GetFinalGLSL(), layout_defs + vert_sampler_defs));
        if (frag) frag->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(frag->GetFinalGLSL(), layout_defs + frag_sampler_defs + frag_mit_defs));
    }

    return true;
}

}  // namespace hgl::graph::mtl
