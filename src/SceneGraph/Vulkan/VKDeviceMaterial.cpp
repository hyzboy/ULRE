#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKShaderModuleMap.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include"VKPipelineLayoutData.h"
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

PipelineLayoutData *CreatePipelineLayoutData(VkDevice device,const MaterialDescriptorManager *desc_manager);

namespace
{
    DescriptorSet *CreateDS(VkDevice device,VkDescriptorPool desc_pool,const PipelineLayoutData *pld,const DescriptorSetType &type)
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

MaterialParameters *VulkanDevice::CreateMP(const MaterialDescriptorManager *desc_manager,const PipelineLayoutData *pld,const DescriptorSetType &desc_set_type)
{
    if(!desc_manager||!pld)return(nullptr);
    RANGE_CHECK_RETURN_NULLPTR(desc_set_type)

    DescriptorSet *ds=CreateDS(attr->device,attr->desc_pool,pld,desc_set_type);

    if(!ds)return(nullptr);

#ifdef _DEBUG
    const U8String addr_string=HexToString<u8char,uint64_t>((uint64_t)(ds->GetDescriptorSet()));

    LogInfo(U8_TEXT("Create [DescriptSets:")+addr_string+U8_TEXT("] OK! Material Name: \"")+(const U8String &)(desc_manager->GetMaterialName())+U8_TEXT("\" Type: ")+(u8char *)(GetDescriptorSetTypeName(desc_set_type)));
#endif//_DEBUG

    return(new MaterialParameters(desc_manager,desc_set_type,ds));
}
VK_NAMESPACE_END
