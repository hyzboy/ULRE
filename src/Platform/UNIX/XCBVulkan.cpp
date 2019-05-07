#include"XCBWindow.h"
#include<hgl/graph/vulkan/VK.h>
#include<vulkan/vulkan_xcb.h>

VK_NAMESPACE_BEGIN
VkSurfaceKHR XCBWindow::CreateSurface(VkInstance vk_inst)
{
    XCBWindow *xcb_win=(XCBWindow *)win;

    VkXcbSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.connection = connection;
    createInfo.window = window;

    VkSurfaceKHR surface;

    VkResult res = vkCreateXcbSurfaceKHR(vk_inst, &createInfo, nullptr, &surface);

    if (res != VK_SUCCESS)
        return(nullptr);

    return(surface);
}
VK_NAMESPACE_END

