#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
bool ShaderModuleMap::Add(const ShaderModule *sm)
{
    if(!sm)return(false);

    const VkShaderStageFlagBits stage=sm->GetStage();

    if(this->KeyExist(stage))return(false);

    return this->Map::Add(stage,sm);
}
VK_NAMESPACE_END