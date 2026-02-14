#include<hgl/platform/Vulkan.h>
#include"WaylandWindow.h"
#include<vulkan/vulkan_wayland.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        WaylandWindow *win=(WaylandWindow *)w;

        VkWaylandSurfaceCreateInfoKHR createInfo;
        createInfo.sType    = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext    = nullptr;
        createInfo.flags    = 0;
        createInfo.display  = win->GetDisplay();
        createInfo.surface  = win->GetSurface();

        VkSurfaceKHR surface;

        VkResult res = vkCreateWaylandSurfaceKHR(vk_inst, &createInfo, nullptr, &surface);

        if (res != VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
