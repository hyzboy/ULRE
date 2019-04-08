#ifndef HGL_GRAPH_VULKAN_PHYSICAL_DEVICE_INCLUDE
#define HGL_GRAPH_VULKAN_PHYSICAL_DEVICE_INCLUDE

#include"VK.h"
#include<hgl/type/List.h>

VK_NAMESPACE_BEGIN

    class Device;

    /**
     * Vulkan物理设备对象封装<br>
     * 注：这个设备可能是图形设备，也可能是计算设备等，我们暂时只支持图形设备
     */
    class PhysicalDevice
    {
        VkPhysicalDevice physical_device;

        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceMemoryProperties memory_properties;

        List<VkQueueFamilyProperties> family_properties;
        int QueueFamilyProperties(VkQueueFlags) const;

    public:

        PhysicalDevice(VkPhysicalDevice pd);
        ~PhysicalDevice();

        Device *CreateGraphicsDevice()const;                                    ///<创建一个图形设备
    };//class PhysicalDevice
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PHYSICAL_DEVICE_INCLUDE
