#pragma once

#include<hgl/vk/VK.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{
struct GraphicsPipelineLayoutData
{
    VkDevice device;

    int vab_count[DESCRIPTOR_SET_TYPE_COUNT];
    VkDescriptorSetLayout layouts[DESCRIPTOR_SET_TYPE_COUNT];

    VkDescriptorSetLayout fin_dsl[DESCRIPTOR_SET_TYPE_COUNT];
    uint32_t fin_dsl_count;

    VkPipelineLayout pipeline_layout;

public:

    int GetVulkanSetIndex(DescriptorSetType t) const
    {
        int idx = 0;
        for (int i = 0; i < (int)t; ++i)
            if (layouts[i]) ++idx;
        return idx;
    }

    ~GraphicsPipelineLayoutData();
};//class GraphicsPipelineLayoutData
}//namespace hgl::graph
