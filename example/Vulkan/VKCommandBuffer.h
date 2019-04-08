#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN
    class CommandBuffer
    {
        VkDevice device;
        VkCommandPool pool;
        VkCommandBuffer buf;

    public:

        CommandBuffer(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb){device=dev;pool=cp;buf=cb;}
        ~CommandBuffer();
    };//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
