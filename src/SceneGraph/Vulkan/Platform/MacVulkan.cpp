#include<hgl/platform/Vulkan.h>
#include"MacWindow.h"
#include<vulkan/vulkan_macos.h>

namespace hgl
{
    VkSurfaceKHR CreateVulkanSurface(VkInstance vk_inst,Window *w)
    {
        if(vk_inst==VK_NULL_HANDLE)return(VK_NULL_HANDLE);
        if(!w)return(VK_NULL_HANDLE);

        MacWindow *win=(MacWindow *)w;

        VkMacOSSurfaceCreateInfoMVK createInfo;
        createInfo.sType    = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        createInfo.pNext    = nullptr;
        createInfo.flags    = 0;
        createInfo.pView    = win->GetView();

        VkSurfaceKHR surface;

        VkResult res = vkCreateMacOSSurfaceMVK(vk_inst, &createInfo, nullptr, &surface);

        if (res != VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return(surface);
    }
}//namespace hgl
