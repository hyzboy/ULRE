#include"VKDescriptorSetLayoutCreater.h"
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
DescriptorSetLayoutCreater *GPUDevice::CreateDescriptorSetLayoutCreater()
{
    return(new DescriptorSetLayoutCreater(attr->device,attr->desc_pool));
}

DescriptorSetLayoutCreater::~DescriptorSetLayoutCreater()
{
    if(pipeline_layout)
        vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    if(dsl)
        vkDestroyDescriptorSetLayout(device,dsl,nullptr);
}

void DescriptorSetLayoutCreater::Bind(const ShaderDescriptorList *sd_list,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    if(!sd_list||sd_list->GetCount()<=0)return;

    const uint32_t binding_count=sd_list->GetCount();

    const uint old_count=layout_binding_list.GetCount();

    layout_binding_list.PreMalloc(old_count+binding_count);

    VkDescriptorSetLayoutBinding *p=layout_binding_list.GetData()+old_count;

    uint fin_count=0;

    for(const ShaderDescriptor &sd:*sd_list)
    {
        //重复的绑定点是有可能存在的，比如CameraInfo在vs/fs中同时存在

        if((!index_by_binding.KeyExist(sd.binding))
         &&(!index_by_binding_ri.KeyExist(sd.binding)))
        {
            p->binding              = sd.binding;
            p->descriptorType       = desc_type;
            p->descriptorCount      = 1;
            p->stageFlags           = stageFlags;
            p->pImmutableSamplers   = nullptr;

            index_by_binding.Add(sd.binding,fin_count+old_count);

            ++p;
            ++fin_count;
        }
    }

    layout_binding_list.SetCount(old_count+fin_count);
}

bool DescriptorSetLayoutCreater::CreatePipelineLayout()
{
    const int count=layout_binding_list.GetCount();

    if(count<=0)
        return(false);

    DescriptorSetLayoutCreateInfo descriptor_layout;

    descriptor_layout.bindingCount  = count;
    descriptor_layout.pBindings     = layout_binding_list.GetData();

    if(dsl)
        vkDestroyDescriptorSetLayout(device,dsl,nullptr);

    if(vkCreateDescriptorSetLayout(device,&descriptor_layout,nullptr,&dsl)!=VK_SUCCESS)
        return(false);

    VkPushConstantRange push_constant_range;

    push_constant_range.stageFlags   = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.size         = MAX_PUSH_CONSTANT_BYTES;
    push_constant_range.offset       = 0;
    
    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

    pPipelineLayoutCreateInfo.setLayoutCount            = 1;
    pPipelineLayoutCreateInfo.pSetLayouts               = &dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = &push_constant_range;

    if(vkCreatePipelineLayout(device,&pPipelineLayoutCreateInfo,nullptr,&pipeline_layout)!=VK_SUCCESS)
        return(false);

    return(true);
}

DescriptorSets *DescriptorSetLayoutCreater::Create()
{
    if(!pipeline_layout||!dsl)
        return(nullptr);

    const int count=layout_binding_list.GetCount();

    if(count<=0)
        return(nullptr);

    DescriptorSetAllocateInfo alloc_info;

    alloc_info.descriptorPool       = pool;
    alloc_info.descriptorSetCount   = 1;
    alloc_info.pSetLayouts          = &dsl;

    VkDescriptorSet desc_set;

    if(vkAllocateDescriptorSets(device,&alloc_info,&desc_set)!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSets(device,count,pipeline_layout,desc_set,&index_by_binding));
}
VK_NAMESPACE_END