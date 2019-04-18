#ifndef HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE

#include"VK.h"
#include"VKDescriptorSetLayout.h"
VK_NAMESPACE_BEGIN
class PipelineLayout
{
    VkDevice device;
    VkPipelineLayout layout;

public:

    PipelineLayout(VkDevice dev,VkPipelineLayout pl){device=dev;layout=pl;}
    ~PipelineLayout();
};//class PipelineLayout

PipelineLayout *CreatePipelineLayout(VkDevice dev,const DescriptorSetLayout &dsl);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
