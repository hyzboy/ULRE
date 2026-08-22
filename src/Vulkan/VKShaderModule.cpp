#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKVertexInputFormat.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

namespace hgl::graph{
struct ShaderModuleCreateInfo:public vkstruct_flag<VkShaderModuleCreateInfo,VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO>
{
public:

    ShaderModuleCreateInfo(const uint32_t *spv_data,const size_t spv_size)
    {
        codeSize=spv_size;
        pCode   =spv_data;
    }
};//struct ShaderModuleCreateInfo

ShaderModule *VulkanDevice::CreateShaderModule(VkShaderStageFlagBits shader_stage_flag_bit,const uint32_t *spv_data,const size_t spv_size)
{
    if(!spv_data||spv_size<4)return(nullptr);

    PipelineShaderStageCreateInfo *pss_ci=new PipelineShaderStageCreateInfo(shader_stage_flag_bit);

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
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)(uintptr_t)stage_create_info->module);

    vkDestroyShaderModule(device,stage_create_info->module,nullptr);
    //这里不用删除stage_create_info，材质中会删除的
}
}//namespace hgl::graph
