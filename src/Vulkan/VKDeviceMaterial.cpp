#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>

namespace hgl::graph{

GraphicsPipelineLayoutData *CreateGraphicsPipelineLayoutData(VkDevice device,const MaterialDescriptorManager *desc_manager);

namespace
{
    DescriptorSet *CreateDS(VkDevice device,VkDescriptorPool desc_pool,const GraphicsPipelineLayoutData *pld,const DescriptorSetType &type)
    {
        RANGE_CHECK_RETURN_NULLPTR(type);

        const uint32_t vab_count=pld->vab_count[size_t(type)];

        if(!vab_count)
            return(nullptr);

        DescriptorSetAllocateInfo alloc_info;

        alloc_info.descriptorPool       = desc_pool;
        alloc_info.descriptorSetCount   = 1;
        alloc_info.pSetLayouts          = pld->layouts+size_t(type);

        VkDescriptorSet desc_set;

        if(vkAllocateDescriptorSets(device,&alloc_info,&desc_set)!=VK_SUCCESS)
            return(nullptr);

        return(new DescriptorSet(device,vab_count,pld->pipeline_layout,desc_set));
    }
}//namespace

MaterialParameters *VulkanDevice::CreateMP(const MaterialDescriptorManager *desc_manager,const GraphicsPipelineLayoutData *pld,const DescriptorSetType &desc_set_type)
{
    if(!desc_manager||!pld)return(nullptr);
    RANGE_CHECK_RETURN_NULLPTR(desc_set_type)

    DescriptorSet *ds=CreateDS(attr->device,attr->desc_pool,pld,desc_set_type);

    if(!ds)return(nullptr);

#ifdef _DEBUG
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << reinterpret_cast<uint64_t>(ds->GetDescriptorSet());
    const std::string addr_string = oss.str();

    LogInfo(std::string("Create [DescriptSets:") + addr_string + "] OK! MaterialTemplate Name: \"" + desc_manager->GetMaterialName().c_str() + "\" Type: " + GetDescriptorSetTypeName(desc_set_type));
#endif//_DEBUG

    return(new MaterialParameters(desc_manager,desc_set_type,ds));
}
}//namespace hgl::graph
