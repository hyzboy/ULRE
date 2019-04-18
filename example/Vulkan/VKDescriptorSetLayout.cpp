#include"VKDescriptorSetLayout.h"

VK_NAMESPACE_BEGIN
DescriptorSetLayout::~DescriptorSetLayout()
{
    const int count=desc_set_layout_list.GetCount();

    if(count>0)
    {
        VkDescriptorSetLayout *dsl=desc_set_layout_list.GetData();

        for(int i=0;i<count;i++)
        {
            vkDestroyDescriptorSetLayout(device,*dsl,nullptr);
            ++dsl;
        }
    }
}

void DescriptorSetLayoutCreater::Bind(const int binding,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = binding;
    layout_binding.descriptorType = desc_type;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = stageFlags;
    layout_binding.pImmutableSamplers = nullptr;

    layout_binding_list.Add(layout_binding);
}

DescriptorSetLayout *DescriptorSetLayoutCreater::Creater()
{
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = nullptr;
    descriptor_layout.bindingCount = layout_binding_list.GetCount();
    descriptor_layout.pBindings = layout_binding_list.GetData();

    List<VkDescriptorSetLayout> dsl_list;

    dsl_list.SetCount(layout_binding_list.GetCount());
    if(vkCreateDescriptorSetLayout(device,&descriptor_layout, nullptr, dsl_list.GetData())!=VK_SUCCESS)
        return(false);

    return(new DescriptorSetLayout(device,dsl_list));
}
VK_NAMESPACE_END
