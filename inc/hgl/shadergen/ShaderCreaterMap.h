#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreaterMap:public ObjectMap<VkShaderStageFlagBits,ShaderCreater>
{
public:

    using ObjectMap<VkShaderStageFlagBits,ShaderCreater>::ObjectMap;

    bool Add(ShaderCreater *sc)
    {
        if(!sc)return(false);

        VkShaderStageFlagBits flag=sc->GetShaderStage();

        if(KeyExist(flag))
            return(false);

        ObjectMap<VkShaderStageFlagBits,ShaderCreater>::Add(flag,sc);
        return(true);
    }
};
SHADERGEN_NAMESPACE_END
