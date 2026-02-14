#ifndef HGL_GRAPH_VULKAN_COMPUTE_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_COMPUTE_PIPELINE_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/type/String.h>

VK_NAMESPACE_BEGIN

class Material;

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

private:

    friend class VulkanDevice;

    ComputePipeline(const AnsiString &n, VkDevice dev, VkPipeline p, VkPipelineLayout pl)
    {
        name = n;
        device = dev;
        pipeline = p;
        pipeline_layout = pl;
    }

public:

    virtual ~ComputePipeline();

    const AnsiString &GetName() const { return name; }
    operator VkPipeline() { return pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return pipeline_layout; }
};//class ComputePipeline

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMPUTE_PIPELINE_INCLUDE
