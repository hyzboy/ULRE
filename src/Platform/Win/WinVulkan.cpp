#include"WinWindow.h"
#include<vulkan/vulkan_win32.h>

namespace hgl
{
    VkSurfaceKHR WinWindow::CreateSurface(VkInstance vk_inst) 
    {
        VkWin32SurfaceCreateInfoKHR createInfo={};
        createInfo.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext=nullptr;
        createInfo.hinstance=hInstance;
        createInfo.hwnd=win_hwnd;

        VkSurfaceKHR surface;

        VkResult res=vkCreateWin32SurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

        if(res!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl