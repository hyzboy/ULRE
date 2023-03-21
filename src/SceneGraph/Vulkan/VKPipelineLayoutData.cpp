#include"VKPipelineLayoutData.h"
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialDescriptorManager.h>

VK_NAMESPACE_BEGIN
PipelineLayoutData *GPUDevice::CreatePipelineLayoutData(const MaterialDescriptorManager *mds)
{
    PipelineLayoutData *pld=hgl_zero_new<PipelineLayoutData>();

    if(mds)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            const DescriptorSetLayoutCreateInfo *dslci=mds->GetDSLCI((DescriptorSetType)i);

            if(!dslci||dslci->bindingCount<=0)
                continue;

            if(pld->layouts[i])
                vkDestroyDescriptorSetLayout(attr->device,pld->layouts[i],nullptr);

            if(vkCreateDescriptorSetLayout(attr->device,dslci,nullptr,pld->layouts+i)!=VK_SUCCESS)
            {
                delete pld;
                return(nullptr);
            }

            pld->binding_count[i]=dslci->bindingCount;

            pld->fin_dsl[pld->fin_dsl_count]=pld->layouts[i];
            ++pld->fin_dsl_count;
        }

        if(pld->fin_dsl_count<=0)
        {
            delete pld;
            return(nullptr);
        }
    }
    else
    {
        //没有任何DescriptorSet的情况也是存在的
    }

    //VkPushConstantRange push_constant_range;

    //push_constant_range.stageFlags   = VK_SHADER_STAGE_VERTEX_BIT;
    //push_constant_range.size         = MAX_PUSH_CONSTANT_BYTES;
    //push_constant_range.offset       = 0;
    
    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

    pPipelineLayoutCreateInfo.setLayoutCount            = pld->fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = pld->fin_dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 0;//1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = nullptr;//&push_constant_range;

    pld->device=attr->device;

    if(vkCreatePipelineLayout(attr->device,&pPipelineLayoutCreateInfo,nullptr,&(pld->pipeline_layout))!=VK_SUCCESS)
    {
        delete pld;
        return(nullptr);
    }
    
    return(pld);
}

PipelineLayoutData::~PipelineLayoutData()
{
    vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(layouts[i])
            vkDestroyDescriptorSetLayout(device,layouts[i],nullptr);
}
VK_NAMESPACE_END

