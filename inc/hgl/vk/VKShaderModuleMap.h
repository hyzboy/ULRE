#pragma once

#include<hgl/vk/VKNamespace.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{

using namespace hgl;

class ShaderModuleMap:public UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>
{
public:

    using UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>::UnorderedMap;
    ~ShaderModuleMap()=default;

    bool Add(const ShaderModule *sm);
};//class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
}//namespace hgl::graph
