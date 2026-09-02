#include<hgl/platform/Vulkan.h>
#include"XCBWindow.h"
#include<vulkan/vulkan_xcb.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        // 通过 Window 虚函数取原生句柄，无需知道具体窗口类
        xcb_connection_t *connection=(xcb_connection_t *)w->GetNativeDisplay();
        xcb_window_t      window     =(xcb_window_t)(uintptr_t)w->GetNativeHandle();

        if(!connection||!window)
            return(VK_NULL_HANDLE);

        VkXcbSurfaceCreateInfoKHR createInfo;
        createInfo.sType        = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext        = nullptr;
        createInfo.flags        = 0;
        createInfo.connection   = connection;
        createInfo.window       = window;

        VkSurfaceKHR surface;

        VkResult res = vkCreateXcbSurfaceKHR(vk_inst, &createInfo, nullptr, &surface);

        if (res != VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
