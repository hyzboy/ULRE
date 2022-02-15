#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKShaderResource.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSets.h>
VK_NAMESPACE_BEGIN
struct PipelineLayoutData
{
    VkDevice device;

    int binding_count[size_t(DescriptorSetsType::RANGE_SIZE)];
    VkDescriptorSetLayout layouts[size_t(DescriptorSetsType::RANGE_SIZE)];

    VkDescriptorSetLayout fin_dsl[size_t(DescriptorSetsType::RANGE_SIZE)];
    uint32_t fin_dsl_count;

    VkPipelineLayout pipeline_layout;

public:

    ~PipelineLayoutData();
};//class PipelineLayoutData
VK_NAMESPACE_END