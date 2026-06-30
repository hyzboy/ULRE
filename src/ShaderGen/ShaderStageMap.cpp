#include<hgl/shadergen/ShaderStageMap.h>
#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{

bool ShaderStageMap::Add(ShaderCreateInfo *sc)
{
    if(!sc)
        return false;

    ShaderStage flag=sc->GetShaderStage();

    if(ContainsKey(flag))
        return false;

    map.emplace(flag,sc);
    return true;
}

}}//namespace hgl::graph
