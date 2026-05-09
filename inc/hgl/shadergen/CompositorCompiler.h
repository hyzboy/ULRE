#pragma once

/// CompositorCompiler.h — StaticMaterialDef → MaterialCreateInfo 编译器接口
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 当前路径为 pipeline-first：
///   1. 基于 StaticMaterialDef 生成/校验 MaterialCreateConfig 与描述符模型
///   2. 注入 layout/sampler/schema 相关 GLSL 定义
///   3. 编译各阶段 GLSL 为 SPIR-V，并返回 MaterialCreateInfo*

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

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const MaterialCreateConfig *config);

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material3DCreateConfig *config = nullptr);

bool InjectLayoutDefines(MaterialCreateInfo &mci);

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics = nullptr);

bool BuildCompileCompositorShadowPipelineReport(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialCreateConfig *config,
    CompileCompositorShadowBuildReport &report);

bool BuildCompileCompositorShadowPipelineReport(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const Material3DCreateConfig *config,
    CompileCompositorShadowBuildReport &report);

}//namespace hgl::graph::mtl
