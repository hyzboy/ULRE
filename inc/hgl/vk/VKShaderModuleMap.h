#pragma once

#include<hgl/vk/VKNamespace.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>

VK_NAMESPACE_BEGIN

using namespace hgl;

class ShaderModuleMap:public UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>
{
public:

    using UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>::UnorderedMap;
    ~ShaderModuleMap()=default;

    bool Add(const ShaderModule *sm);
};//class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
VK_NAMESPACE_END
