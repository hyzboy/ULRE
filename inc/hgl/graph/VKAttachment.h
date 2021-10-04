#ifndef HGL_VULKAN_ATTACHMENT_INCLUDE
#define HGL_VULKAN_ATTACHMENT_INCLUDE
#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
struct Attachment
{
    VkFormat                format          =VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits   samples         =VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags       usage           =VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout           initial_layout  =VK_IMAGE_LAYOUT_UNDEFINED;
};//struct Attachment
VK_NAMESPACE_END
#endif//HGL_VULKAN_ATTACHMENT_INCLUDE
