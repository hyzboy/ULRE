#ifndef HGL_VULKAN_ATTACHMENT_INCLUDE
#define HGL_VULKAN_ATTACHMENT_INCLUDE
#include<hgl/vk/VK.h>
namespace hgl::graph{
struct Attachment
{
    VkFormat                format          =VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits   samples         =VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags       usage           =VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout           initial_layout  =VK_IMAGE_LAYOUT_UNDEFINED;
};//struct Attachment
}//namespace hgl::graph
#endif//HGL_VULKAN_ATTACHMENT_INCLUDE
