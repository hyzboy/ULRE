#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
struct ShaderModuleCreateInfo:public vkstruct_flag<VkShaderModuleCreateInfo,VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO>
{
public:

    ShaderModuleCreateInfo(const uint32_t *spv_data,const size_t spv_size)
    {
        codeSize=spv_size;
        pCode   =spv_data;
    }
};//struct ShaderModuleCreateInfo

ShaderModule *GPUDevice::CreateShaderModule(VkShaderStageFlagBits shader_stage,const uint32_t *spv_data,const size_t spv_size)
{
    if(!spv_data||spv_size<4)return(nullptr);

    PipelineShaderStageCreateInfo *pss_ci=new PipelineShaderStageCreateInfo(shader_stage);

    ShaderModuleCreateInfo moduleCreateInfo(spv_data,spv_size);

    if(vkCreateShaderModule(attr->device,&moduleCreateInfo,nullptr,&(pss_ci->module))!=VK_SUCCESS)
        return(nullptr);

    return(new ShaderModule(attr->device,pss_ci));
}

ShaderModule::ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *sci)
{
    device=dev;
    ref_count=0;

    stage_create_info=sci;
}

ShaderModule::~ShaderModule()
{
    vkDestroyShaderModule(device,stage_create_info->module,nullptr);
    //这里不用删除stage_create_info，材质中会删除的
}
VK_NAMESPACE_END
