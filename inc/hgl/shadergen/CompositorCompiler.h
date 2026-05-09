#pragma once

/// CompositorCompiler.h — StaticMaterialDef → MaterialCreateInfo 编译器接口
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 按 def 的 ubo/ssbo/texture_samplers 三组定义构建 MaterialDescriptorDB
///   2. 使用 SetFinalGLSL + CompilePreparedShaderSources 直接编译
///   3. 填充并返回 MaterialCreateInfo*

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/shadergen/ShaderBuildRouteSwitch.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include <filesystem>
#include <string>
#include <vector>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
struct Material3DCreateConfig;
struct Material2DCreateConfig;
class MaterialCreateInfo;

struct CompileCompositorRoutePlan
{
    ShaderBuildRoute preferred_route = ShaderBuildRoute::Pipeline;
    bool allow_pipeline_fallback = false;
    bool can_export_readiness = false;
    bool can_emit_baseline_artifacts = false;
    std::string rationale;
};

struct CompileCompositorRouteDecision
{
    ShaderBuildRoute resolved_route = ShaderBuildRoute::Pipeline;
    bool will_use_legacy_now = false;
    bool pipeline_trial_requested = false;
    bool fallback_to_legacy = false;
    std::string rationale;
};

struct CompileCompositorShadowBuildReport
{
    MaterialCreateConfig pipeline_config;
    ShaderBuildDescriptorSpec descriptor_spec;
    ShaderGenResult<ShaderBuildResult> result;
    ShaderBuildRouteEvaluation evaluation;
    std::string summary;

    CompileCompositorShadowBuildReport()
        : pipeline_config(PrimitiveType::Triangles,false)
    {
    }
};

struct CompileCompositorTrialBatchReport
{
    size_t total_count = 0;
    size_t legacy_success_count = 0;
    size_t legacy_failure_count = 0;
    size_t pipeline_trial_success_count = 0;
    size_t pipeline_trial_failure_count = 0;
    size_t baseline_report_count = 0;
    size_t baseline_compare_success_count = 0;
    bool aggregate_report_written = false;
    std::vector<std::string> legacy_failed_materials;
    std::vector<std::string> pipeline_trial_failed_materials;
};

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const MaterialCreateConfig *config);

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config = nullptr);

bool InjectLayoutDefines(MaterialCreateInfo &mci);

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics = nullptr);

CompileCompositorRoutePlan BuildCompileCompositorRoutePlan();
CompileCompositorRouteDecision ResolveCompileCompositorRouteDecision(const ShaderBuildSwitchConfig *switch_config=nullptr);
std::string GetCompileCompositorRouteDecisionSummary(const CompileCompositorRouteDecision &decision);
CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const MaterialCreateConfig *config);
CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const Material3DCreateConfig *config = nullptr);

bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir = "build/shadergen_trial/reports");
bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root = "build/shadergen_trial/pipeline");

std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                       const char *material_name);
bool EnsureDirectoryExists(const char *dir);
bool WriteTextFile(const std::filesystem::path &path,const std::string &text);
std::string SanitizeArtifactName(const char *text);
std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec);
std::string BuildPipelineConfigText(const MaterialCreateConfig &config);
std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report);
std::string BuildSpirvHexText(const ShaderBinary &binary);
bool WriteCompileCompositorPreparedTreeInternal(const MaterialCreateInfo &mci,
                                                const char *material_name,
                                                const char *legacy_root);

}//namespace hgl::graph::mtl
