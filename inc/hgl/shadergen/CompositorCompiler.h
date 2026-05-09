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
#include <string>
#include <vector>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
struct Material3DCreateConfig;
struct Material2DCreateConfig;
class MaterialCreateInfo;

struct CompileCompositorRoutePlan
{
    ShaderBuildRoute preferred_route = ShaderBuildRoute::LegacyMaterialCreateInfo;
    bool allow_pipeline_fallback = true;
    bool can_export_readiness = false;
    bool can_emit_baseline_artifacts = false;
    std::string rationale;
};

struct CompileCompositorRouteDecision
{
    ShaderBuildRoute resolved_route = ShaderBuildRoute::LegacyMaterialCreateInfo;
    bool will_use_legacy_now = true;
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

struct CompileCompositorTrialBatchItem
{
    const StaticMaterialDef *def = nullptr;
    std::string vs_glsl;
    std::string fs_glsl;
    const MaterialCreateConfig *config = nullptr;
    std::string material_name_override;
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

/**
 * 编译 Compositor 模板产出的完整 GLSL → MaterialCreateInfo*。
 *
 * 使用 SetFinalGLSL() + CompilePreparedShaderSources() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param def       材质定义（descriptor/vertex/MI 元数据）
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    运行时配置（可选）
 * @return          编译好的 MaterialCreateInfo*; 失败返回 nullptr
 */
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

/**
 * Resort descriptors, build layout contract, emit layout/sampler defines,
 * and inject them into the VS/FS GLSL stored in @p mci.
 *
 * Call this after all descriptors, vertex inputs, and MI have been set up
 * but before CompilePreparedShaderSources().
 */
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
bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root = "build/shadergen_trial");
bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root = "build/shadergen_trial/pipeline");
bool WriteCompileCompositorLegacyTree(const MaterialCreateInfo &mci,
                                      const char *material_name,
                                      const char *legacy_root = "build/shadergen_trial/legacy");
std::string BuildCompileCompositorBaselineCompareCommand(const char *material_name,
                                                         const char *trial_root = "build/shadergen_trial");
bool RunCompileCompositorBaselineCompare(const char *material_name,
                                         const char *trial_root = "build/shadergen_trial");
bool WriteCompileCompositorTrialAggregateReport(const char *trial_root = "build/shadergen_trial",
                                               const CompileCompositorTrialBatchReport *report = nullptr);
std::string GetCompileCompositorTrialBatchSummary(const CompileCompositorTrialBatchReport &report);

}//namespace hgl::graph::mtl
