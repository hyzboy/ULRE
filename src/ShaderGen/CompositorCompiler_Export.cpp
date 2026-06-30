#include <hgl/shadergen/CompositorCompiler.h>
#include<hgl/log/Logger.h>
#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>

#include <cstdio>
#include <string>

namespace hgl::graph::mtl
{
MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const MaterialCreateConfig *config)
{
    ShaderBuildPipeline pipeline;
    const MaterialCreateConfig build_cfg = ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(def,config);

    auto result = pipeline.BuildProduct(def,build_cfg,profile,vs_glsl,fs_glsl);
    if(!result.success)
    {
        GLogError(
                     "[CompileCompositorMaterial] material=%s build failed: %s\n",
                     def.name ? def.name : "<unnamed>",
                     result.diagnostics.empty() ? "<unknown>" : result.diagnostics.front().message.c_str());
        return nullptr;
    }

    return result.value;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
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
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material2DCreateConfig *config)
{
    return CompileCompositorMaterial(profile,
                                     def,
                                     vs_glsl,
                                     fs_glsl,
                                     static_cast<const MaterialCreateConfig *>(config));
}

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics)
{
    if(diagnostics)
        diagnostics->clear();

    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg(def.primitive_type,false);
    auto result = pipeline.PrepareMaterialCreateInfo(def,cfg,nullptr,vs_glsl,fs_glsl);
    if(!result.success || !result.value)
    {
        if(diagnostics && !result.diagnostics.empty())
            *diagnostics = result.diagnostics.front().message;
        return false;
    }

    ShaderCreateInfoVertex *vert = result.value->GetVertexShader();
    ShaderCreateInfo *frag = result.value->GetStageShader(ShaderStage::Fragment);
    out_vs_glsl = vert ? vert->GetFinalGLSL() : std::string();
    out_fs_glsl = frag ? frag->GetFinalGLSL() : std::string();

    delete result.value;
    return true;
}

bool BuildCompileCompositorShadowPipelineReport(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialCreateConfig *config,
    CompileCompositorShadowBuildReport &report)
{
    ShaderBuildPipeline pipeline;
    const MaterialCreateConfig pipeline_cfg = ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(def,config);
    const ShaderBuildDescriptorSpec descriptor_spec = ShaderBuildPipeline::BuildDescriptorSpecFromStaticMaterialDef(def);

    report.pipeline_config = pipeline_cfg;
    report.descriptor_spec = descriptor_spec;
    report.result = pipeline.Build(pipeline_cfg,profile,&descriptor_spec);
    report.evaluation = EvaluateShaderBuildResultForRouteSwitch(report.result);
    report.summary = GetShaderBuildRouteEvaluationSummary(report.evaluation);
    return report.result.success;
}

bool BuildCompileCompositorShadowPipelineReport(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const Material3DCreateConfig *config,
    CompileCompositorShadowBuildReport &report)
{
    return BuildCompileCompositorShadowPipelineReport(profile,
                                                      def,
                                                      static_cast<const MaterialCreateConfig *>(config),
                                                      report);
}

}//namespace hgl::graph::mtl
