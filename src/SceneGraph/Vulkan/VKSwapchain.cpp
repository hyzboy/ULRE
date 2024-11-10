#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN
Swapchain::~Swapchain()
{
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(sc_fbo,image_count);
    SAFE_CLEAR(sc_depth);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(sc_color,image_count)

    if(swap_chain)
    {
        vkDestroySwapchainKHR(device,swap_chain,VK_NULL_HANDLE);
        swap_chain=VK_NULL_HANDLE;
    }

    image_count=0;
}
VK_NAMESPACE_END
