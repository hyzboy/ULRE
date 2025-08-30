#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
class ShaderCreateInfoMap:public ObjectMap<ShaderStage,ShaderCreateInfo>
{
public:

    using ObjectMap<ShaderStage,ShaderCreateInfo>::ObjectMap;

    bool Add(ShaderCreateInfo *sc)
    {
        if(!sc)return(false);

        ShaderStage flag=sc->GetShaderStage();

        if(ContainsKey(flag))
            return(false);

        ObjectMap<ShaderStage,ShaderCreateInfo>::Add(flag,sc);
        return(true);
    }
};
}}//namespace hgl::graph
