#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<vulkan/vulkan_win32.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        // 通过 Window 虚函数取原生句柄（WinWindow / QtWindow / GTK 窗口等任何实现了
        // GetNativeHandle/GetNativeDisplay 的窗口类均可，无需知道具体类型）
        const HWND hwnd      =(HWND)w->GetNativeHandle();
        const HINSTANCE hinst=(HINSTANCE)w->GetNativeDisplay();

        if(!hwnd)
            return(VK_NULL_HANDLE);

        VkWin32SurfaceCreateInfoKHR createInfo;
        createInfo.sType    =VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext    =nullptr;
        createInfo.flags    =0;
        createInfo.hinstance=hinst?hinst:GetModuleHandleW(nullptr);
        createInfo.hwnd     =hwnd;

        VkSurfaceKHR surface;

        VkResult res=vkCreateWin32SurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

        if(res!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
