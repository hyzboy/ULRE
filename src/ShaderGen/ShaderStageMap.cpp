#include<hgl/shadergen/ShaderStageMap.h>

namespace hgl{namespace graph{

bool ShaderStageMap::Add(ShaderCreateInfo *sc)
{
    return Add(std::unique_ptr<ShaderCreateInfo>(sc));
}

bool ShaderStageMap::Add(std::unique_ptr<ShaderCreateInfo> sc)
{
    if(!sc)
        return false;

    const ShaderStage flag=sc->GetShaderStage();
    return Add(flag,std::move(sc));
}

}}//namespace hgl::graph
