#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph::shadergen{
    using namespace hgl::graph::mtl;

bool ShaderCreateInfoMap::Add(ShaderCreateInfo *sc)
{
    if(!sc)
        return false;

    ShaderStage flag=sc->GetShaderStage();

    if(ContainsKey(flag))
        return false;

    map.emplace(flag,sc);
    return true;
}

}}//namespace hgl::graph::shadergen
