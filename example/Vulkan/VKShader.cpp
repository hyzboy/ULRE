#include"VKShader.h"

VK_NAMESPACE_BEGIN

VkShaderModule CreateShaderModule(VkDevice device,const uint32_t *spv_data,const uint32_t spv_size,const VkShaderStageFlagBits shader_stage_bit)
{
    VkPipelineShaderStageCreateInfo shader_stage;
    shader_stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.pNext=nullptr;
    shader_stage.pSpecializationInfo=nullptr;
    shader_stage.flags=0;
    shader_stage.stage=shader_stage_bit;
    shader_stage.pName="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext=nullptr;
    moduleCreateInfo.flags=0;
    moduleCreateInfo.codeSize=spv_size;
    moduleCreateInfo.pCode=spv_data;

    VkShaderModule shader_module;

    if(vkCreateShaderModule(device,&moduleCreateInfo,nullptr,&shader_module)==VK_SUCCESS)
        return shader_module;

    return nullptr;
}

VK_NAMESPACE_END
