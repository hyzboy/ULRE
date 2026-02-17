#include<hgl/vk/VKManagerExtensions.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/RenderPassManager.h>

namespace hgl::graph{

namespace ManagerExtensions
{

// ============================================
// TextureManager Extensions Implementation
// ============================================

ImageView *TextureManager_CreateSwapchainImageView(
    TextureManager *tm,
    VkImage vk_image,
    VkFormat format,
    VkImageAspectFlags aspect_mask)
{
    if (!tm || !vk_image)
        return nullptr;

    // TODO: Implement swapchain ImageView creation
    // This should:
    // 1. Create a VkImageView for the swapchain image
    // 2. Register it with TextureManager for ownership tracking
    // 3. Return the ImageView pointer
    
    // For now, return null to indicate not yet implemented
    return nullptr;
}

void TextureManager_ReleaseImageView(
    TextureManager *tm,
    ImageView *img_view)
{
    if (!tm || !img_view)
        return;

    // TODO: Implement ImageView release
    // This should:
    // 1. Unregister ImageView from TextureManager
    // 2. Destroy the VkImageView
    // 3. Free the ImageView object
}

// ============================================
// RenderTargetManager Extensions Implementation
// ============================================

Framebuffer *RenderTargetManager_CreateFramebuffer(
    RenderTargetManager *rtm,
    RenderPass *render_pass,
    ImageView **color_views,
    uint32_t color_count,
    ImageView *depth_view)
{
    if (!rtm || !render_pass || !color_views || color_count == 0)
        return nullptr;

    // TODO: Implement framebuffer creation
    // This should:
    // 1. Call RenderTargetManager::CreateFBO() with proper parameters
    // 2. Register returned Framebuffer with RenderTargetManager
    // 3. Return the Framebuffer pointer
    
    return nullptr;
}

void RenderTargetManager_ReleaseFramebuffer(
    RenderTargetManager *rtm,
    Framebuffer *framebuffer)
{
    if (!rtm || !framebuffer)
        return;

    // TODO: Implement Framebuffer release
    // This should:
    // 1. Unregister Framebuffer from RenderTargetManager
    // 2. Destroy the VkFramebuffer
    // 3. Free the Framebuffer object
}

// ============================================
// RenderPassManager Extensions Implementation
// ============================================

RenderPass *RenderPassManager_AcquireSwapchainRenderPass(
    RenderPassManager *rpm,
    VkFormat color_format,
    VkFormat depth_format,
    uint32_t subpass_count)
{
    if (!rpm)
        return nullptr;

    // TODO: Implement swapchain RenderPass acquisition
    // This should:
    // 1. Create or retrieve a RenderPass for swapchain rendering
    // 2. Handle color_format and depth_format specifications
    // 3. Return the RenderPass (which is reference-counted/cached)
    
    return nullptr;
}

void RenderPassManager_ReleaseRenderPass(
    RenderPassManager *rpm,
    RenderPass *render_pass)
{
    if (!rpm || !render_pass)
        return;

    // TODO: Implement RenderPass release
    // This should:
    // 1. Decrease reference count for the RenderPass
    // 2. If reference count reaches zero, destroy the VkRenderPass
    // 3. Free the RenderPass object if needed
}

// ============================================
// Validation Helper Implementation
// ============================================

bool ValidateFrameResources(const FrameResources &frame)
{
    // Check critical resources
    if (frame.vk_image == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: vk_image is null");
        return false;
    }

    if (frame.image_view == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: image_view is null");
        return false;
    }

    if (frame.framebuffer == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: framebuffer is null");
        return false;
    }

    if (frame.render_pass == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: render_pass is null");
        return false;
    }

    if (frame.cmd_buffer == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: cmd_buffer is null");
        return false;
    }

    if (frame.queue == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: queue is null");
        return false;
    }

    if (frame.image_acquired_semaphore == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: image_acquired_semaphore is null");
        return false;
    }

    if (frame.render_complete_semaphore == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: render_complete_semaphore is null");
        return false;
    }

    if (frame.fence == VK_NULL_HANDLE)
    {
        LOG_WARNING("FrameResources: fence is null");
        return false;
    }

    return true;
}

} // namespace ManagerExtensions

}//namespace hgl::graph
