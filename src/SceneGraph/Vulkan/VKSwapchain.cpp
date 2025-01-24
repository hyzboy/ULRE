#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKRenderPass.h>

VK_NAMESPACE_BEGIN
Swapchain::~Swapchain()
{
    SAFE_CLEAR_ARRAY(sc_image);

    if(swap_chain)
    {
        vkDestroySwapchainKHR(device,swap_chain,VK_NULL_HANDLE);
        swap_chain=VK_NULL_HANDLE;
    }

    image_count=0;
}
VK_NAMESPACE_END
