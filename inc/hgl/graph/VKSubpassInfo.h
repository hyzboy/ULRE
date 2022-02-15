#ifndef HGL_VULKAN_SUBPASS_INFO_INCLUDE
#define HGL_VULKAN_SUBPASS_INFO_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/List.h>
VK_NAMESPACE_BEGIN
struct SubpassInfo
{
    List<uint32_t> input_attachments;
    List<uint32_t> output_attachments;

    struct
    {
        List<uint32_t> attachments;
    }color;

    struct
    {
        bool                    enable;
        uint32_t                attachment;
	    VkResolveModeFlagBits   mode;
    }depth_stencil;
};//struct SubpassInfo
VK_NAMESPACE_END
#endif//HGL_VULKAN_SUBPASS_INFO_INCLUDE
