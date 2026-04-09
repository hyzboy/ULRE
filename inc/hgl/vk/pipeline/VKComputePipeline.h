#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>

namespace hgl::graph{

class MaterialTemplate;

/**
 * Compute Pipeline类
 * 用于管理计算着色器管线
 */
class ComputePipeline
{
    VkDevice device;
    std::string name;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;

private:

    friend class VulkanDevice;

    ComputePipeline(const std::string &n, VkDevice dev, VkPipeline p, VkPipelineLayout pl)
    {
        name = n;
        device = dev;
        pipeline = p;
        pipeline_layout = pl;
    }

public:

    virtual ~ComputePipeline();

    const std::string &GetName() const { return name; }
    operator VkPipeline() { return pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return pipeline_layout; }
};//class ComputePipeline

}//namespace hgl::graph
