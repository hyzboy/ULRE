#include"VKDescriptorSets.h"
#include"VKDevice.h"

VK_NAMESPACE_BEGIN
DescriptorSets::~DescriptorSets()
{
    // 这里注释掉是因为从来不见那里的范便有FREE过，但又有vkFreeDescriptorSets这个函数。如发现此注释，请使用工具查是否有资源泄露
    {
        //const int count=desc_sets.GetCount();

        //if(count>0)
        //    vkFreeDescriptorSets(device->GetDevice(),device->GetDescriptorPool(),count,desc_sets.GetData());
    }
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    const int count=desc_set_layout_list.GetCount();

    if(count>0)
    {
        VkDescriptorSetLayout *dsl=desc_set_layout_list.GetData();

        for(int i=0;i<count;i++)
        {
            vkDestroyDescriptorSetLayout(device->GetDevice(),*dsl,nullptr);
            ++dsl;
        }
    }
}

DescriptorSets *DescriptorSetLayout::CreateSets()const
{
    const int count=desc_set_layout_list.GetCount();

    if(count<=0)
        return(nullptr);

    VkDescriptorSetAllocateInfo alloc_info[1];
    alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info[0].pNext = nullptr;
    alloc_info[0].descriptorPool = device->GetDescriptorPool();
    alloc_info[0].descriptorSetCount = count;
    alloc_info[0].pSetLayouts = desc_set_layout_list.GetData();

    List<VkDescriptorSet> desc_set;

    desc_set.SetCount(count);

    if(vkAllocateDescriptorSets(device->GetDevice(), alloc_info, desc_set.GetData())!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSets(device,desc_set));
}

void DescriptorSetLayoutCreater::Bind(const uint32_t binding,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding;
    layout_binding.descriptorType = desc_type;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = stageFlags;
    layout_binding.pImmutableSamplers = nullptr;

    layout_binding_list.Add(layout_binding);
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

    List<VkDescriptorSetLayout> dsl_list;

    dsl_list.SetCount(count);
    if(vkCreateDescriptorSetLayout(device->GetDevice(),&descriptor_layout, nullptr, dsl_list.GetData())!=VK_SUCCESS)
        return(nullptr);

    return(new DescriptorSetLayout(device,dsl_list));
}
VK_NAMESPACE_END
