#include<hgl/shadergen/ShaderStageMap.h>

namespace hgl{namespace graph{

bool ShaderStageMap::Add(ShaderCreateInfo *sc)
{
    std::unique_ptr<ShaderCreateInfo> owned(sc);

    if(!owned)
        return false;

    ShaderStage flag=owned->GetShaderStage();

    if(ContainsKey(flag))
        return false;

    map.emplace(flag,std::move(owned));
    return true;
}

}}//namespace hgl::graph
