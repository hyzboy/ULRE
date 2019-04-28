#include"VKDescriptorSets.h"
#include"VKDevice.h"

VK_NAMESPACE_BEGIN
namespace
{
    void DestroyDescriptorSetLayout(VkDevice device,List<VkDescriptorSetLayout> &dsl_list)
    {
        const int count=dsl_list.GetCount();

        if(count>0)
        {
            VkDescriptorSetLayout *dsl=dsl_list.GetData();

            for(int i=0;i<count;i++)
            {
                vkDestroyDescriptorSetLayout(device,*dsl,nullptr);
                ++dsl;
            }

            dsl_list.Clear();
        }
    }
}//namespace

DescriptorSetLayout::~DescriptorSetLayout()
{
    // 这里注释掉是因为从来不见那里的范例有FREE过，但又有vkFreeDescriptorSets这个函数。如发现此注释，请使用工具查是否有资源泄露
    //{
    //const int count=desc_sets.GetCount();

    //if(count>0)
    //    vkFreeDescriptorSets(device->GetDevice(),device->GetDescriptorPool(),count,desc_sets.GetData());
    //}

    DestroyDescriptorSetLayout(*device,desc_set_layout_list);
}

bool DescriptorSetLayout::UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info)
{
    int index;
    
    if(!binding_index.Get(binding,index))
        return(false);

    VkDescriptorSet set;
    if(!desc_sets.Get(index,set))
        return(false);

    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;    
    writeDescriptorSet.dstSet = set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = buf_info;
    writeDescriptorSet.dstBinding = binding;

    vkUpdateDescriptorSets(device->GetDevice(), 1, &writeDescriptorSet, 0, nullptr);
    return(true);
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
    
    binding_index.Add(binding,index);
}

void DescriptorSetLayoutCreater::Bind(const uint32_t *binding,const uint32_t count,VkDescriptorType desc_type,VkShaderStageFlagBits stageFlags)
{
    const int old_count=layout_binding_list.GetCount();

    layout_binding_list.SetCount(old_count+count);

    VkDescriptorSetLayoutBinding *p=layout_binding_list.GetData()+old_count;

    for(int i=old_count;i<old_count+count;i++)
    {
        p->binding = *binding;
        p->descriptorType = desc_type;
        p->descriptorCount = 1;
        p->stageFlags = stageFlags;
        p->pImmutableSamplers = nullptr;

        binding_index.Add(*binding,i);

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

    List<VkDescriptorSetLayout> dsl_list;

    dsl_list.SetCount(count);
    if(vkCreateDescriptorSetLayout(device->GetDevice(),&descriptor_layout, nullptr, dsl_list.GetData())!=VK_SUCCESS)
        return(nullptr);

    VkDescriptorSetAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = device->GetDescriptorPool();
    alloc_info.descriptorSetCount = count;
    alloc_info.pSetLayouts = dsl_list.GetData();

    List<VkDescriptorSet> desc_set;

    desc_set.SetCount(count);

    if(vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, desc_set.GetData())!=VK_SUCCESS)
    {
        DestroyDescriptorSetLayout(*device,dsl_list);
        return(nullptr);
    }

    return(new DescriptorSetLayout(device,dsl_list,desc_set,binding_index));
}
VK_NAMESPACE_END
