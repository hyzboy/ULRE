#pragma once

#include<hgl/shadergen/ShaderBuildPipeline.h>
#include<string>
#include<vector>

namespace hgl::graph
{
enum class ShaderBuildRoute
{
    LegacyMaterialCreateInfo = 0,
    Pipeline = 1
};

struct ShaderBuildSwitchConfig
{
    bool enable_pipeline = false;
    bool allow_fallback_to_legacy = true;
};

struct ShaderBuildRouteEvaluation
{
    bool pipeline_ready = false;
    bool baseline_compare_ready = false;
    bool schema_aware_material_instance = false;
    std::vector<std::string> reasons;
};

ShaderBuildRoute ResolveShaderBuildRoute(const ShaderBuildSwitchConfig *config=nullptr);
const char *GetShaderBuildRouteName(const ShaderBuildRoute route);
ShaderBuildRouteEvaluation EvaluateShaderBuildResultForRouteSwitch(const ShaderGenResult<ShaderBuildResult> &result);
}//namespace hgl::graph
