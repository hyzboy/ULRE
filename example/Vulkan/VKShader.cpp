#include"VKShader.h"

VK_NAMESPACE_BEGIN

ShaderCreater::~ShaderCreater()
{
    const int count=shader_stage_list.GetCount();

    if(count>0)
    {
        VkPipelineShaderStageCreateInfo *ss=shader_stage_list.GetData();
        for(int i=0;i<count;i++)
        {
            vkDestroyShaderModule(device,ss->module,nullptr);
            ++ss;
        }
    }
}

bool ShaderCreater::Add(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size)
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
    moduleCreateInfo.pCode=(const uint32_t *)spv_data;

    if(vkCreateShaderModule(device,&moduleCreateInfo,nullptr,&shader_stage.module)!=VK_SUCCESS)
        return(false);

    shader_stage_list.Add(shader_stage);

    return(true);
}
VK_NAMESPACE_END
