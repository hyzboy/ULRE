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
}//namespace hgl::graph
