#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/vk/pipeline/VKPipelineLayoutData.h>

namespace hgl::graph{

PipelineLayoutData *CreatePipelineLayoutData(VkDevice device,const MaterialDescriptorManager *desc_manager);

namespace
{
    DescriptorSet *CreateDS(VkDevice device,VkDescriptorPool desc_pool,const PipelineLayoutData *pld,const DescriptorSetType &type)
    {
        RANGE_CHECK_RETURN_NULLPTR(type);

        const uint32_t vab_count=pld->vab_count[size_t(type)];

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

MaterialParameters *VulkanDevice::CreateMP(const MaterialDescriptorManager *desc_manager,const PipelineLayoutData *pld,const DescriptorSetType &desc_set_type)
{
    if(!desc_manager||!pld)return(nullptr);
    RANGE_CHECK_RETURN_NULLPTR(desc_set_type)

    DescriptorSet *ds=CreateDS(attr->device,attr->desc_pool,pld,desc_set_type);

    if(!ds)return(nullptr);

#ifdef _DEBUG
    const U8String addr_string=HexToString<u8char,uint64_t>((uint64_t)(ds->GetDescriptorSet()));

    LogInfo(U8_TEXT("Create [DescriptSets:")+addr_string+U8_TEXT("] OK! ShaderProgram Name: \"")+(const U8String &)(desc_manager->GetMaterialName())+U8_TEXT("\" Type: ")+(u8char *)(GetDescriptorSetTypeName(desc_set_type)));
#endif//_DEBUG

    return(new MaterialParameters(desc_manager,desc_set_type,ds));
}
}//namespace hgl::graph
