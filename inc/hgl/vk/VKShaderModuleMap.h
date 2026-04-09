#pragma once

#include<hgl/vk/VKShaderModule.h>
#include<ankerl/unordered_dense.h>

namespace hgl::graph{

using namespace hgl;

class ShaderModuleMap:public ankerl::unordered_dense::map<VkShaderStageFlagBits,const ShaderModule *>
{
public:

    using Base=ankerl::unordered_dense::map<VkShaderStageFlagBits,const ShaderModule *>;
    using Base::Base;
    ~ShaderModuleMap()=default;

    int GetCount() const
    {
        return static_cast<int>(this->size());
    }

    bool ContainsKey(const VkShaderStageFlagBits &stage) const
    {
        return this->find(stage) != this->end();
    }

    bool Add(const ShaderModule *sm);
};//class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
}//namespace hgl::graph
