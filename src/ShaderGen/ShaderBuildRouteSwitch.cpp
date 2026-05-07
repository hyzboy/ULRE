#include<hgl/shadergen/ShaderBuildRouteSwitch.h>
#include<cstdlib>
#include<cstring>

namespace hgl::graph
{
static bool IsEnabledText(const char *text)
{
    if(!text || !*text)
        return false;

    if(std::strcmp(text,"1")==0)
        return true;

    if(std::strcmp(text,"true")==0)
        return true;

    if(std::strcmp(text,"TRUE")==0)
        return true;

    if(std::strcmp(text,"on")==0)
        return true;

    if(std::strcmp(text,"ON")==0)
        return true;

    if(std::strcmp(text,"pipeline")==0)
        return true;

    if(std::strcmp(text,"PIPELINE")==0)
        return true;

    return false;
}

ShaderBuildRoute ResolveShaderBuildRoute(const ShaderBuildSwitchConfig *config)
{
    if(config && config->enable_pipeline)
        return ShaderBuildRoute::Pipeline;

    const char *env_value=std::getenv("ULRE_SHADERGEN_PIPELINE");
    if(IsEnabledText(env_value))
        return ShaderBuildRoute::Pipeline;

    return ShaderBuildRoute::LegacyMaterialCreateInfo;
}

const char *GetShaderBuildRouteName(const ShaderBuildRoute route)
{
    switch(route)
    {
        case ShaderBuildRoute::Pipeline: return "Pipeline";
        default: return "LegacyMaterialCreateInfo";
    }
}

ShaderBuildRouteEvaluation EvaluateShaderBuildResultForRouteSwitch(const ShaderGenResult<ShaderBuildResult> &result)
{
    ShaderBuildRouteEvaluation evaluation{};

    if(!result.success)
    {
        evaluation.reasons.emplace_back("pipeline build did not succeed");
        return evaluation;
    }

    if(!result.value.layout_finalized)
        evaluation.reasons.emplace_back("descriptor layout not finalized");

    if(result.value.binaries.empty())
        evaluation.reasons.emplace_back("no compiled shader binaries produced");

    bool has_schema_diag=false;
    for(const auto &d:result.diagnostics)
    {
        if(d.subject=="ShaderBuildPipeline.MaterialInstance.Schema")
            has_schema_diag=true;
    }

    evaluation.schema_aware_material_instance =
        result.value.material_instance.schema!=mtl::ShaderDataSchema::None &&
        !result.value.material_instance.schema_file.empty() &&
        has_schema_diag;

    evaluation.pipeline_ready = result.success && result.value.layout_finalized && !result.value.binaries.empty();
    evaluation.baseline_compare_ready = evaluation.pipeline_ready;

    if(result.value.material_instance.schema!=mtl::ShaderDataSchema::None && !evaluation.schema_aware_material_instance)
        evaluation.reasons.emplace_back("material_instance schema-aware diagnostics/model are incomplete");

    return evaluation;
}
}//namespace hgl::graph
