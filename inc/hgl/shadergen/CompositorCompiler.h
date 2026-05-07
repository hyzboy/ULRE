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

namespace hgl::graph::mtl{

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
    ShaderGenResult<ShaderBuildResult> result;
    ShaderBuildRouteEvaluation evaluation;
    std::string summary;
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
    const Material3DCreateConfig *config = nullptr);

/**
 * CompileCompositorMaterial — 2D 材质重载
 *
 * 将 Material2DCreateConfig 映射到内部 Material3DCreateConfig 后调用主版本。
 * camera/sky 默认关闭，其余字段从 2D 配置继承。
 */
MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config);

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
                                                                              const Material3DCreateConfig *config = nullptr);
bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir = "build/shadergen_trial/reports");
bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root = "build/shadergen_trial");

// diagnostics may also contain non-fatal inference mismatch warnings on success.

}//namespace hgl::graph::mtl
