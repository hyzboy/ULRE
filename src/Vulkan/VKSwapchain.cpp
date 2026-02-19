#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

namespace hgl::graph{
SwapchainImage::~SwapchainImage()
{
    // SwapchainImage owns these resources, cleanup on destruction
    SAFE_CLEAR(fbo);
    SAFE_CLEAR(depth);
    SAFE_CLEAR(color);

    // cmd_buf is shared with RenderTargetData, but SwapchainImage owns it
    // RenderTargetData::cmd_buf should be set to nullptr before SwapchainImage destruction
    SAFE_CLEAR(cmd_buf);
}

Swapchain::~Swapchain()
{
    // Delete SwapchainImage array
    // Note: SwapchainImage destructor does NOT delete cmd_buf (owned by queue)
    delete[] sc_image;
    sc_image = nullptr;

    if(swap_chain)
    {
        VulkanDevice *owner = VulkanDevice::FromDevice(device);
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_SWAPCHAIN_KHR, (uint64_t)(uintptr_t)swap_chain);

        vkDestroySwapchainKHR(device,swap_chain,VK_NULL_HANDLE);
        swap_chain=VK_NULL_HANDLE;
    }

    image_count=0;
}
}//namespace hgl::graph
