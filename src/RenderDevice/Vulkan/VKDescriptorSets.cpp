#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
VkDescriptorSet DescriptorSets::GetDescriptorSet(const uint32_t binding)
{
    int index;

    if(!index_by_binding.Get(binding,index))
        return(nullptr);

    return desc_set_list[index];
}

DescriptorSets::~DescriptorSets()
{
    //if(count>0)
    //    vkFreeDescriptorSets(device->GetDevice(),device->GetDescriptorPool(),count,desc_set_list);

    delete[] desc_set_list;
}

bool DescriptorSets::UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info)
{
    VkDescriptorSet set=GetDescriptorSet(binding);

    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = buf_info;
    writeDescriptorSet.dstBinding = binding;

    vkUpdateDescriptorSets(*device,1,&writeDescriptorSet,0,nullptr);
    return(true);
}
VK_NAMESPACE_END
