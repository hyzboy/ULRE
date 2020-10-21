#include<hgl/graph/VKSwapchain.h>

VK_NAMESPACE_BEGIN
Swapchain::~Swapchain()
{
    SAFE_CLEAR(sc_depth);
    sc_color.Clear();

    if(swap_chain)
    {
        vkDestroySwapchainKHR(device,swap_chain,VK_NULL_HANDLE);
        swap_chain=VK_NULL_HANDLE;
    }

    swap_chain_count=0;
}
VK_NAMESPACE_END
