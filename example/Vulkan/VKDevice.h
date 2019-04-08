#ifndef HGL_GRAPH_VULKAN_DEVICE_INCLUDE
#define HGL_GRAPH_VULKAN_DEVICE_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN

    class CommandBuffer;

    /**
     * Vulkan设备对象封装
     */
    class Device
    {
        VkDevice device;
        VkCommandPool cmd_pool;                                                 ///<命令池，用于创建命令缓冲区。由于不知道创建多个是否有好处，所以暂时设计为一个设备只有一个。

    public:

        Device(VkDevice,int);
        virtual ~Device();

        CommandBuffer *CreateCommandBuffer();
    };//class Device
VK_NAMESPACE_END

#endif//HGL_GRAPH_VULKAN_DEVICE_INCLUDE
