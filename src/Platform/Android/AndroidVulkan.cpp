#include"AndroidWindow.h"
#include<vulkan/vulkan_android.h>

namespace hgl
{
    VkSurfaceKHR AndroidWindow::CreateSurface(VkInstance vk_inst)
    {
        PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurfaceKHR;

        GET_INSTANCE_PROC_ADDR(vk_inst,CreateAndroidSurfaceKHR);

        if(!CreateAndroidSurfaceKHR)
            return(nullptr);

        VkAndroidSurfaceCreateInfoKHR createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext = nullptr;
        createInfo.window = AndroidGetApplicationWindow();

        VkSurfaceKHR surface;

        VkResult res=CreateAndroidSurfaceKHR(vk_inst,&createInfo,nullptr,&surface);

        if(res!=VK_SUCCESS)
            return(nullptr);

        return(surface);
    }
}//namespace hgl
