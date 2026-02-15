#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

VK_NAMESPACE_BEGIN
SwapchainImage::~SwapchainImage()
{
    delete cmd_buf;
}

Swapchain::~Swapchain()
{
    SAFE_CLEAR_ARRAY(sc_image);

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
VK_NAMESPACE_END
