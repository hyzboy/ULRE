#pragma once

#include<hgl/vk/VK.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/type/UnorderedMap.h>
#include<vector>

namespace hgl::graph{
struct PipelineLayoutData
{
    VkDevice device;

    int vab_count[DESCRIPTOR_SET_TYPE_COUNT];
    VkDescriptorSetLayout layouts[DESCRIPTOR_SET_TYPE_COUNT];

    VkDescriptorSetLayout fin_dsl[DESCRIPTOR_SET_TYPE_COUNT];
    uint32_t fin_dsl_count;

    int bindless_set_index = -1;    ///< bindless layout 在 fin_dsl 中的位置，-1=不含

    // 占位空 layout（填充中间缺失 set 用，析构时释放）
    std::vector<VkDescriptorSetLayout> placeholder_layouts;

    VkPipelineLayout pipeline_layout;

public:

    ~PipelineLayoutData();
};//class PipelineLayoutData
}//namespace hgl::graph
