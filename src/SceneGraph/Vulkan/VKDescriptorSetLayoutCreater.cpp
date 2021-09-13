#include"VKDescriptorSetLayoutCreater.h"
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialDescriptorSets.h>

VK_NAMESPACE_BEGIN
DescriptorSetLayoutCreater *GPUDevice::CreateDescriptorSetLayoutCreater(const MaterialDescriptorSets *mds)
{
    return(new DescriptorSetLayoutCreater(attr->device,attr->desc_pool,mds));
}

DescriptorSetLayoutCreater::DescriptorSetLayoutCreater(VkDevice dev,VkDescriptorPool dp,const MaterialDescriptorSets *_mds)
{
    hgl_zero(fin_dsl);
    fin_dsl_count=0;
    device=dev;
    pool=dp;
    mds=_mds;

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        layouts[i]=nullptr;
}

DescriptorSetLayoutCreater::~DescriptorSetLayoutCreater()
{
    if(pipeline_layout)
        vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(layouts[i])
            vkDestroyDescriptorSetLayout(device,layouts[i],nullptr);
}

bool DescriptorSetLayoutCreater::CreatePipelineLayout()
{
    fin_dsl_count=0;

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        const int count=mds->GetSetBindingCount((DescriptorSetType)i);

        if(count<=0)
            continue;

        DescriptorSetLayoutCreateInfo descriptor_layout;

        descriptor_layout.bindingCount  = count;
        descriptor_layout.pBindings     = mds->GetSetBindingList((DescriptorSetType)i);

        if(layouts[i])
            vkDestroyDescriptorSetLayout(device,layouts[i],nullptr);

        if(vkCreateDescriptorSetLayout(device,&descriptor_layout,nullptr,layouts+i)!=VK_SUCCESS)
            return(false);

        fin_dsl[fin_dsl_count]=layouts[i];
        ++fin_dsl_count;
    }

    if(fin_dsl_count<=0)
        return(false);

    //VkPushConstantRange push_constant_range;

    //push_constant_range.stageFlags   = VK_SHADER_STAGE_VERTEX_BIT;
    //push_constant_range.size         = MAX_PUSH_CONSTANT_BYTES;
    //push_constant_range.offset       = 0;
    
    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

    pPipelineLayoutCreateInfo.setLayoutCount            = fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = fin_dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 0;//1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = nullptr;//&push_constant_range;

    if(vkCreatePipelineLayout(device,&pPipelineLayoutCreateInfo,nullptr,&pipeline_layout)!=VK_SUCCESS)
        return(false);

    return(true);
}

DescriptorSets *DescriptorSetLayoutCreater::Create(const DescriptorSetType &type)const
{
    if(!pipeline_layout)
        return(nullptr);

    ENUM_CLASS_RANGE_ERROR_RETURN_NULLPTR(DescriptorSetType,type);

    const uint32_t count=mds->GetSetBindingCount(type);

    if(!count)
        return(nullptr);

    DescriptorSetAllocateInfo alloc_info;

    alloc_info.descriptorPool       = pool;
    alloc_info.descriptorSetCount   = 1;
    alloc_info.pSetLayouts          = layouts+size_t(type);

    VkDescriptorSet desc_set;

    if(vkAllocateDescriptorSets(device,&alloc_info,&desc_set)!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSets(device,count,pipeline_layout,desc_set));//,bm));
}
VK_NAMESPACE_END

