#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSets.h>
VK_NAMESPACE_BEGIN
struct PipelineLayoutData
{
    VkDevice device;

    int binding_count[DESCRIPTOR_SET_TYPE_COUNT];
    VkDescriptorSetLayout layouts[DESCRIPTOR_SET_TYPE_COUNT];

    VkDescriptorSetLayout fin_dsl[DESCRIPTOR_SET_TYPE_COUNT];
    uint32_t fin_dsl_count;

    VkPipelineLayout pipeline_layout;

public:

    ~PipelineLayoutData();
};//class PipelineLayoutData
VK_NAMESPACE_END