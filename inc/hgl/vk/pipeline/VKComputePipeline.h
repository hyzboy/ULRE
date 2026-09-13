#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>

namespace hgl::graph{

class ShaderProgram;

/**
 * Compute Pipeline类
 * 用于管理计算着色器管线
 */
class ComputePipeline
{
    VkDevice device;
    AnsiString name;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    bool owns_pipeline_layout=false;    ///< true：析构时一并销毁 pipeline_layout（专用 layout）

private:

    friend class VulkanDevice;

    ComputePipeline(const AnsiString &n, VkDevice dev, VkPipeline p, VkPipelineLayout pl, const bool owns_pl=false)
    {
        name = n;
        device = dev;
        pipeline = p;
        pipeline_layout = pl;
        owns_pipeline_layout = owns_pl;
    }

public:

    virtual ~ComputePipeline();

    const AnsiString &GetName() const { return name; }
    operator VkPipeline() { return pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return pipeline_layout; }
};//class ComputePipeline

}//namespace hgl::graph
