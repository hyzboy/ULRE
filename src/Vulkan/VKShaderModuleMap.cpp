#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKShaderModule.h>

namespace hgl::graph{
bool ShaderModuleMap::Add(const ShaderModule *sm)
{
    if(!sm)return(false);

    const VkShaderStageFlagBits stage=sm->GetStage();

    if(this->ContainsKey(stage))return(false);

    return UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>::Add(stage,sm);
}
}//namespace hgl::graph
