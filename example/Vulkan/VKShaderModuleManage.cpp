#include"VKShaderModuleManage.h"
#include"VKShaderModule.h"

VK_NAMESPACE_BEGIN
int ShaderModuleManage::CreateShader(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size)
{
    VkPipelineShaderStageCreateInfo *shader_stage=new VkPipelineShaderStageCreateInfo;
    shader_stage->sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage->pNext=nullptr;
    shader_stage->pSpecializationInfo=nullptr;
    shader_stage->flags=0;
    shader_stage->stage=shader_stage_bit;
    shader_stage->pName="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext=nullptr;
    moduleCreateInfo.flags=0;
    moduleCreateInfo.codeSize=spv_size;
    moduleCreateInfo.pCode=(const uint32_t *)spv_data;

    if(vkCreateShaderModule(device,&moduleCreateInfo,nullptr,&(shader_stage->module))!=VK_SUCCESS)
        return(-1);

    ShaderModule *sm=new ShaderModule(shader_count,shader_stage);

    shader_list.Add(shader_count,sm);

    return shader_count++;
}

const ShaderModule *ShaderModuleManage::GetShader(int id)
{
    ShaderModule *sm;

    if(!shader_list.Get(id,sm))
        return nullptr;
    
    sm->IncRef();
    return sm;
}

bool ShaderModuleManage::ReleaseShader(const ShaderModule *const_sm)
{
    if(!const_sm)return;

    ShaderModule *sm;

    if(!shader_list.Get(const_sm->GetID(),sm))
        return nullptr;
    
    if(sm!=const_sm)
        return(false);
    
    sm->DecRef();
    return(true);
}
VK_NAMESPACE_END
