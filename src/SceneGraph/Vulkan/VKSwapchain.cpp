#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN
SwapchainImage::~SwapchainImage()
{
    delete cmd_buf;
    delete fbo;

    if(depth)
    delete depth;

    delete color;
}

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
