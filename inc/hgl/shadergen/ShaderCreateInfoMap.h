#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
class ShaderCreateInfoMap:public ObjectMap<VkShaderStageFlagBits,ShaderCreateInfo>
{
public:

    using ObjectMap<VkShaderStageFlagBits,ShaderCreateInfo>::ObjectMap;

    bool Add(ShaderCreateInfo *sc)
    {
        if(!sc)return(false);

        VkShaderStageFlagBits flag=sc->GetShaderStage();

        if(ContainsKey(flag))
            return(false);

        ObjectMap<VkShaderStageFlagBits,ShaderCreateInfo>::Add(flag,sc);
        return(true);
    }
};
}}//namespace hgl::graph
