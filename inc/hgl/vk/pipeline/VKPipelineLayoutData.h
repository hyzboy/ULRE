#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/type/UnorderedMap.h>

VK_NAMESPACE_BEGIN
struct PipelineLayoutData
{
    VkDevice device;

    int vab_count[DESCRIPTOR_SET_TYPE_COUNT];
    VkDescriptorSetLayout layouts[DESCRIPTOR_SET_TYPE_COUNT];

    VkDescriptorSetLayout fin_dsl[DESCRIPTOR_SET_TYPE_COUNT];
    uint32_t fin_dsl_count;

    VkPipelineLayout pipeline_layout;

public:

    ~PipelineLayoutData();
};//class PipelineLayoutData
VK_NAMESPACE_END
