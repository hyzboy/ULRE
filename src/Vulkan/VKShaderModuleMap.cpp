#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKShaderModule.h>

VK_NAMESPACE_BEGIN
bool ShaderModuleMap::Add(const ShaderModule *sm)
{
    if(!sm)return(false);

    const VkShaderStageFlagBits stage=sm->GetStage();

    if(this->ContainsKey(stage))return(false);

    return UnorderedMap<VkShaderStageFlagBits,const ShaderModule *>::Add(stage,sm);
}
VK_NAMESPACE_END
