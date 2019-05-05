#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
namespace
{
    void DestroyDescriptorSetLayout(VkDevice device,const int count,VkDescriptorSetLayout *dsl_list)
    {
        if(count<=0)
            return;

        for(int i=0;i<count;i++)
        {
            vkDestroyDescriptorSetLayout(device,*dsl_list,nullptr);
            ++dsl_list;
        }
    }
}//namespace

VkDescriptorSet DescriptorSetLayout::GetDescriptorSet(const uint32_t binding)
{
    int index;

    if(!index_by_binding.Get(binding,index))
        return(nullptr);

    return desc_set_list[index];
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    //if(count>0)
    //    vkFreeDescriptorSets(device->GetDevice(),device->GetDescriptorPool(),count,desc_set_list);

    if(pipeline_layout)
        vkDestroyPipelineLayout(*device,pipeline_layout,nullptr);

    delete[] desc_set_list;
    DestroyDescriptorSetLayout(*device,count,desc_set_layout_list);
    delete[] desc_set_layout_list;
}

void DescriptorSetLayoutCreater::Bind(const uint32_t binding,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding;
    layout_binding.descriptorType = desc_type;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = stageFlags;
    layout_binding.pImmutableSamplers = nullptr;

    const int index=layout_binding_list.Add(layout_binding);

    index_by_binding.Add(binding,index);
}

void DescriptorSetLayoutCreater::Bind(const uint32_t *binding,const uint32_t count,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    const uint old_count=layout_binding_list.GetCount();

    layout_binding_list.SetCount(old_count+count);

    VkDescriptorSetLayoutBinding *p=layout_binding_list.GetData()+old_count;

    for(uint i=old_count;i<old_count+count;i++)
    {
        p->binding = *binding;
        p->descriptorType = desc_type;
        p->descriptorCount = 1;
        p->stageFlags = stageFlags;
        p->pImmutableSamplers = nullptr;

        index_by_binding.Add(*binding,i);

        ++binding;
        ++p;
    }
}

DescriptorSetLayout *DescriptorSetLayoutCreater::Create()
{
    const int count=layout_binding_list.GetCount();

    if(count<=0)
        return(nullptr);

    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = nullptr;
    descriptor_layout.bindingCount = count;
    descriptor_layout.pBindings = layout_binding_list.GetData();

    VkDescriptorSetLayout *dsl_list=new VkDescriptorSetLayout[count];

    if(vkCreateDescriptorSetLayout(*device,&descriptor_layout, nullptr, dsl_list)!=VK_SUCCESS)
    {
        delete[] dsl_list;
        return(nullptr);
    }

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = count;
    pPipelineLayoutCreateInfo.pSetLayouts = dsl_list;

    VkPipelineLayout pipeline_layout;

    if(vkCreatePipelineLayout(*device, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout)!=VK_SUCCESS)
    {
        delete[] dsl_list;
        return(nullptr);
    }

    VkDescriptorSetAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = device->GetDescriptorPool();
    alloc_info.descriptorSetCount = count;
    alloc_info.pSetLayouts = dsl_list;

    VkDescriptorSet *desc_set=new VkDescriptorSet[count];

    if(vkAllocateDescriptorSets(*device,&alloc_info,desc_set)!=VK_SUCCESS)
    {
        vkDestroyPipelineLayout(*device,pipeline_layout,nullptr);
        delete[] desc_set;
        DestroyDescriptorSetLayout(*device,count,dsl_list);
        delete[] dsl_list;
        return(nullptr);
    }

    return(new DescriptorSetLayout(device,count,dsl_list,pipeline_layout,desc_set,index_by_binding));
}
VK_NAMESPACE_END
