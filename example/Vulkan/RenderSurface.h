#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"VKCommandBuffer.h"

VK_NAMESPACE_BEGIN
class RenderSurface
{
	Window *win;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkSurfaceKHR surface;

    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;

    List<VkQueueFamilyProperties> family_properties;
    List<VkBool32> supports_present;

	List<VkSurfaceFormatKHR> surface_formts;
	VkSurfaceCapabilitiesKHR surface_caps;
	List<VkPresentModeKHR> present_modes;

    uint32_t family_index;
    VkDevice device;
    VkCommandPool cmd_pool;                                                 ///<命令池，用于创建命令缓冲区。由于不知道创建多个是否有好处，所以暂时设计为一个设备只有一个。

protected:

    int QueueFamilyProperties(VkQueueFlags) const;

    bool CreateDevice();
    bool CreateCommandPool();

public:

    RenderSurface(Window *,VkInstance,VkPhysicalDevice);
    virtual ~RenderSurface();

    VkPhysicalDevice GetPhysicalDevice() { return physical_device; }
    VkSurfaceKHR GetSurface() { return surface; }

public:

    CommandBuffer *CreateCommandBuffer();
};//class RenderSurface
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
