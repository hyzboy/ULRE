#include<hgl/platform/Vulkan.h>
#include"AndroidWindow.h"
#include<vulkan/vulkan_android.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        AndroidWindow *win=(Android *)w;

        VkAndroidSurfaceCreateInfoKHR createInfo;
        createInfo.sType    = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext    = nullptr;
        createInfo.flags    = 0;
        createInfo.window   = win->GetWindow();

        VkSurfaceKHR surface;

        VkResult res=CreateAndroidSurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

        if(res!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
