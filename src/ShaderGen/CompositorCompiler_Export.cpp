#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialVariantRegistry.h>
#include "3d/StandardDescriptorBuilder.h"
#include "3d/Build3DCommon.h"
#include "3d/StandardVariantRouter.h"
#include "3d/MaterialFactory3DCommon.h"
#include "2d/Build2DCommon.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <span>
#include <string>

namespace hgl::graph::mtl {

bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic);
bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic);
bool HasPerMaterialDescriptor(const StaticMaterialDef &def);
bool ResolveConfiguredCameraRequirement(const Material3DCreateConfig &cfg);
bool ResolveConfiguredSkyRequirement(const Material3DCreateConfig &cfg);
void AppendDiagnosticLine(std::string *diagnostics, const std::string &line);
void EmitInferenceMismatchDiagnostics(
    const StaticMaterialDef &def,
    const Material3DCreateConfig &cfg,
    const bool infer_has_camera,
    const bool infer_has_sky,
    std::string *diagnostics);
std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def);
std::string BuildShaderDataSchemaIncludeText(const ShaderDataSchemaInfo &schema_info);
ShaderBuildDescriptorSpec BuildDescriptorSpecFromStaticMaterialDef(const StaticMaterialDef &def);
void AppendShadowBuildDiagnostics(std::string &text,
                                  const ShaderGenResult<ShaderBuildResult> &result);
std::string SanitizeArtifactName(const char *text);
bool EnsureDirectoryExists(const char *dir);
bool WriteTextFile(const std::filesystem::path &path,const std::string &text);
void AppendTrialBatchSummaryFields(std::string &text,
                                   const CompileCompositorTrialBatchReport &report,
                                   const bool markdown_list);
std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                       const char *material_name);
std::string BuildTrialAggregateReportText(const std::filesystem::path &trial_root,
                                          const CompileCompositorTrialBatchReport *trial_report);
std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec);
const char *GetShaderStageArtifactName(const ShaderStage stage);
std::string BuildDescriptorInfoText(const MaterialDescriptorDB &descriptor_db,
                                    const DescriptorBindingSlots &binding_contract);
std::string BuildMaterialBlocksText(const MaterialCreateInfo &mci);
bool WriteLegacyShaderArtifacts(const std::filesystem::path &material_root,
                                const MaterialCreateInfo &mci);
bool WriteCompileCompositorPreparedTreeInternal(const MaterialCreateInfo &mci,
                                                const char *material_name,
                                                const char *legacy_root);
std::string BuildPipelineConfigText(const MaterialCreateConfig &config);
std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report);
std::string BuildSpirvHexText(const ShaderBinary &binary);
std::string BuildBaselineCompareReportText(const CompileCompositorShadowBuildReport &report,
                                           const char *material_name,
                                           const bool direct_compile_success,
                                           const char *direct_compile_summary);
bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir);
bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               const bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root);
bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root);
std::string BuildCompileCompositorBaselineCompareCommand(const char *material_name,
                                                         const char *trial_root);
bool RunCompileCompositorBaselineCompare(const char *material_name,
                                         const char *trial_root);
bool WriteCompileCompositorTrialAggregateReport(const char *trial_root,
                                                const CompileCompositorTrialBatchReport *report);
std::string GetCompileCompositorTrialBatchSummary(const CompileCompositorTrialBatchReport &report);
CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const MaterialCreateConfig *config);
CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const Material3DCreateConfig *config);
void EmitCompileCompositorShadowTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                               const char *material_name);
void EmitCompileCompositorFailureAndTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                                   const bool has_shadow_report,
                                                   const char *material_name,
                                                   const char *failure_text,
                                                   const char *baseline_summary);
void EmitCompileCompositorSuccessAndTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                                   const bool has_shadow_report,
                                                   const MaterialCreateInfo &mci,
                                                   const char *material_name,
                                                   const char *baseline_summary);
void EmitCompileCompositorPrepareFailure(const CompileCompositorShadowBuildReport &shadow_report,
                                         const bool has_shadow_report,
                                         const char *material_name,
                                         const std::string &diagnostics);
CompileCompositorRoutePlan BuildCompileCompositorRoutePlan();
CompileCompositorRouteDecision ResolveCompileCompositorRouteDecision(const ShaderBuildSwitchConfig *switch_config);
std::string GetCompileCompositorRouteDecisionSummary(const CompileCompositorRouteDecision &decision);

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const MaterialCreateConfig *config)
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

        EmitCompileCompositorShadowTrialArtifacts(shadow_report,def.name);
    }

    std::string diagnostics;
    ShaderBuildPipeline pipeline;
    const MaterialCreateConfig build_cfg=ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(def,config);

    if(config && config->kind==ConfigKind::D3)
    {
        const auto *cfg3d=static_cast<const Material3DCreateConfig *>(config);
        const bool infer_has_camera=HasUBOSemantic(def,UBODescriptorSemantic::CameraInfo);
        const bool infer_has_sky=HasUBOSemantic(def,UBODescriptorSemantic::SkyInfo);
        EmitInferenceMismatchDiagnostics(def,*cfg3d,infer_has_camera,infer_has_sky,&diagnostics);
    }

    auto build_result = pipeline.PrepareMaterialCreateInfo(def,
                                                           build_cfg,
                                                           profile,
                                                           vs_glsl,
                                                           fs_glsl);
    if(!build_result.success && !build_result.diagnostics.empty())
    {
        std::string message=build_result.diagnostics.front().message;
        message += " (";
        message += ShaderBuildPipeline::BuildShaderDataSchemaDebugText(def);
        message += ")";
        AppendDiagnosticLine(&diagnostics,message);
    }

    MaterialCreateInfo *mci = build_result.success ? build_result.value : nullptr;
    if (!mci)
    {
        EmitCompileCompositorFailureAndTrialArtifacts(shadow_report,
                                                      has_shadow_report,
                                                      def.name,
                                                      diagnostics.empty() ? "<unknown>" : diagnostics.c_str(),
                                                      diagnostics.empty() ? "PrepareMaterialCreateInfo failed" : diagnostics.c_str());
        return nullptr;
    }

    if (!mci->CompileShaderStagesToSPV())
    {
        std::string failure_text = "CompileShaderStagesToSPV() failed (check GLSLCompiler log) (";
        failure_text += ShaderBuildPipeline::BuildShaderDataSchemaDebugText(def);
        failure_text += ")";
        EmitCompileCompositorFailureAndTrialArtifacts(shadow_report,
                                                      has_shadow_report,
                                                      def.name,
                                                      failure_text.c_str(),
                                                      "CompileShaderStagesToSPV() failed");
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

    EmitCompileCompositorSuccessAndTrialArtifacts(shadow_report,
                                                  has_shadow_report,
                                                  *mci,
                                                  def.name,
                                                  diagnostics.empty() ? "CompileCompositorMaterial legacy compile succeeded" : diagnostics.c_str());

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
        if (diagnostics)
            diagnostics->clear();

        ShaderBuildPipeline pipeline;
        auto build_result = pipeline.PrepareMaterialCreateInfo(def,
                                                               nullptr,
                                                               nullptr,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               diagnostics);
        MaterialCreateInfo *mci = build_result.success ? build_result.value : nullptr;
        if (!mci)
            return false;

        ShaderCreateInfoVertex *vert = mci->GetVertexShader();
        ShaderCreateInfo       *frag = mci->GetStageShader(ShaderStage::Fragment);

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
        else    {
            decision.rationale = "CompileCompositorMaterial remains on Legacy by default in F2.10; route-switch intent is evaluated but not yet wired into the production compile branch."
                                " consider using 'material_instance' in the config to force-enable the pipeline variant for this material.";
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
                                                                              const MaterialCreateConfig *config)
    {
        CompileCompositorShadowBuildReport report{};

        ShaderBuildPipeline pipeline;
    const MaterialCreateConfig pipeline_cfg=ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(def,config);
        const ShaderBuildDescriptorSpec descriptor_spec=BuildDescriptorSpecFromStaticMaterialDef(def);

        report.pipeline_config=pipeline_cfg;
        report.descriptor_spec=descriptor_spec;

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

CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const Material3DCreateConfig *config)
{
    return BuildCompileCompositorShadowPipelineReport(profile,
                                                      def,
                                                      static_cast<const MaterialCreateConfig *>(config));
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

    bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root)
    {
        if(!EnsureDirectoryExists(pipeline_root))
            return false;

        const std::string sanitized_name=SanitizeArtifactName(material_name);
        const std::filesystem::path material_root=std::filesystem::path(pipeline_root)/sanitized_name;

        if(!EnsureDirectoryExists(material_root.string().c_str()))
            return false;

        if(!WriteTextFile(material_root/"descriptor_spec.txt",BuildDescriptorSpecText(report.descriptor_spec)))
            return false;

        if(!WriteTextFile(material_root/"pipeline_config.txt",BuildPipelineConfigText(report.pipeline_config)))
            return false;

        if(!WriteTextFile(material_root/"result_summary.txt",BuildPipelineResultText(report)))
            return false;

        if(!WriteTextFile(material_root/"readiness.txt",report.summary))
            return false;

        if(!WriteTextFile(material_root/"diagnostics.log",BuildShadowDiagnosticsText(report,material_name)))
            return false;

        for(size_t i=0;i<report.result.value.binaries.size();++i)
        {
            const ShaderBinary &binary=report.result.value.binaries[i];
            const std::filesystem::path spv_path=material_root/(std::string("stage_") + std::to_string(i) + ".spv.txt");

            if(!WriteTextFile(spv_path,BuildSpirvHexText(binary)))
                return false;
        }

        return true;
    }

    MaterialCreateInfo *CompileCompositorMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const StaticMaterialDef &    def,
        const std::string &         vs_glsl,
        const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    return CompileCompositorMaterial(profile,
                                     def,
                                     vs_glsl,
                                     fs_glsl,
                                     static_cast<const MaterialCreateConfig *>(config));
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
        const Material2DCreateConfig *config)
    {
    return CompileCompositorMaterial(profile,
                                     def,
                                     vs_glsl,
                                     fs_glsl,
                                     static_cast<const MaterialCreateConfig *>(config));
    }

}  // namespace hgl::graph::mtl
