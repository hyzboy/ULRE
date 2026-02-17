#ifndef HGL_VULKAN_SUBPASS_INFO_INCLUDE
#define HGL_VULKAN_SUBPASS_INFO_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/type/List.h>
namespace hgl::graph{
struct SubpassInfo
{
    ValueArray<uint32_t> input_attachments;
    ValueArray<uint32_t> output_attachments;

    struct
    {
        ValueArray<uint32_t> attachments;
    }color;

    struct
    {
        bool                    enable;
        uint32_t                attachment;
        VkResolveModeFlagBits   mode;
    }depth_stencil;
};//struct SubpassInfo
}//namespace hgl::graph
#endif//HGL_VULKAN_SUBPASS_INFO_INCLUDE
