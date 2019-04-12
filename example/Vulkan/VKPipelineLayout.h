#ifndef HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class PipelineLayout
{
    VkDevice device;
    VkPipelineLayout layout;
    List<VkDescriptorSetLayout> desc_set_layout;

public:

    PipelineLayout();
    ~PipelineLayout();
};//class PipelineLayout
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
