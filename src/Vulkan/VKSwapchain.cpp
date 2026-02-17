#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

VK_NAMESPACE_BEGIN
SwapchainImage::~SwapchainImage()
{
    // cmd_buf is owned by DeviceQueue, not SwapchainImage
    // Do not delete it here
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
VK_NAMESPACE_END
