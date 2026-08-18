#include<hgl/mtl/ShaderCreateInfoMap.h>
#include<hgl/mtl/ShaderCreateInfo.h>

namespace hgl{namespace graph::mtl{
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

}}//namespace hgl::graph::mtl
