#include<hgl/platform/Vulkan.h>
#include<hgl/platform/WinWindow.h>
#include<vulkan/vulkan_win32.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        WinWindow *win=(WinWindow *)w;

        VkWin32SurfaceCreateInfoKHR createInfo;
        createInfo.sType    =VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext    =nullptr;
        createInfo.flags    =0;
        createInfo.hinstance=win->GetInstance();
        createInfo.hwnd     =win->GetWnd();

        VkSurfaceKHR surface;

        VkResult res=vkCreateWin32SurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

        if(res!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl