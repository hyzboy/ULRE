#include<hgl/platform/Vulkan.h>
#include"XCBWindow.h"
#include<vulkan/vulkan_xcb.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        XCBWindow *win=(XCBWindow *)w;

        VkXcbSurfaceCreateInfoKHR createInfo;
        createInfo.sType        = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext        = nullptr;
        createInfo.flags        = 0;
        createInfo.connection   = win->GetConnection();
        createInfo.window       = win->GetWindow();

        VkSurfaceKHR surface;

        VkResult res = vkCreateXcbSurfaceKHR(vk_inst, &createInfo, nullptr, &surface);

        if (res != VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
