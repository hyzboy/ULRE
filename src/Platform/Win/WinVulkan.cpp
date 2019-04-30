#include"WinWindow.h"
#include<hgl/graph/vulkan/VK.h>
#include<vulkan/vulkan_win32.h>

VK_NAMESPACE_BEGIN
VkSurfaceKHR CreateRenderDevice(VkInstance vk_inst,Window *win)
{
    WinWindow *ww=(WinWindow *)win;

    VkWin32SurfaceCreateInfoKHR createInfo={};
    createInfo.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext=nullptr;
    createInfo.hinstance=ww->GetHInstance();
    createInfo.hwnd=ww->GetHWnd();

    VkSurfaceKHR surface;

    VkResult res=vkCreateWin32SurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(surface);
}
VK_NAMESPACE_END
