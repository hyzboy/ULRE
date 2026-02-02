#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl{namespace graph{
class ShaderCreateInfoMap:public UnorderedMap<ShaderStage,ShaderCreateInfo *>
{
public:

    using UnorderedMap<ShaderStage,ShaderCreateInfo *>::UnorderedMap;

    bool Add(ShaderCreateInfo *sc)
    {
        if(!sc)return(false);

        ShaderStage flag=sc->GetShaderStage();

        if(ContainsKey(flag))
            return(false);

        UnorderedMap<ShaderStage,ShaderCreateInfo *>::Add(flag,sc);
        return(true);
    }

    // operator[] for accessing elements by key
    ShaderCreateInfo* operator[](const ShaderStage& key)
    {
        auto* ptr = GetValuePointer(key);
        return ptr ? *ptr : nullptr;
    }

    const ShaderCreateInfo* operator[](const ShaderStage& key) const
    {
        auto* ptr = GetValuePointer(key);
        return ptr ? *ptr : nullptr;
    }
};
}}//namespace hgl::graph
