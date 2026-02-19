#ifndef __VK_MANAGER_EXTENSIONS_H__
#define __VK_MANAGER_EXTENSIONS_H__

#include<hgl/vk/VKFrameData.h>
#include<vulkan/vulkan.h>

namespace hgl::graph{

// Forward declarations
class TextureManager;
class RenderTargetManager;
class RenderPassManager;
class ImageView;
class Framebuffer;
class RenderPass;

/**
 * @class ManagerExtensions
 * @brief Extensions for manager classes to support new swapchain architecture
 *
 * These functions extend the capabilities of the manager classes to handle
 * swapchain-specific resource creation and lifetime management.
 *
 * They bridge the gap between the old and new architecture during the transition.
 */
namespace ManagerExtensions
{
    // ============================================
    // TextureManager Extensions
    // ============================================

    /**
     * @brief Create ImageView for swapchain image
     *
     * TextureManager is responsible for owning and destroying ImageViews.
     * This method creates a view specifically for a swapchain image.
     *
     * @param tm TextureManager instance
     * @param vk_image VkImage to create view for
     * @param format Image format
     * @param aspect_mask Aspect mask (usually COLOR_BIT for swapchain)
     * @return ImageView* owned by TextureManager
     */
    ImageView *TextureManager_CreateSwapchainImageView(
        TextureManager *tm,
        VkImage vk_image,
        VkFormat format,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

    /**
     * @brief Release ImageView ownership
     *
     * Called during swapchain cleanup. Notifies TextureManager that this ImageView
     * should be destroyed.
     *
     * @param tm TextureManager instance
     * @param img_view ImageView to release
     */
    void TextureManager_ReleaseImageView(
        TextureManager *tm,
        ImageView *img_view);

    // ============================================
    // RenderTargetManager Extensions
    // ============================================

    /**
     * @brief Create Framebuffer for swapchain frame
     *
     * RenderTargetManager is responsible for owning and destroying Framebuffers.
     * This method creates a framebuffer for use in a swapchain frame.
     *
     * @param rtm RenderTargetManager instance
     * @param render_pass RenderPass to use (owned by RenderPassManager)
     * @param color_views Array of color ImageViews (owned by TextureManager)
     * @param color_count Number of color views
     * @param depth_view Depth ImageView (owned by TextureManager), may be null
     * @return Framebuffer* owned by RenderTargetManager
     */
    Framebuffer *RenderTargetManager_CreateFramebuffer(
        RenderTargetManager *rtm,
        RenderPass *render_pass,
        ImageView **color_views,
        uint32_t color_count,
        ImageView *depth_view = nullptr);

    /**
     * @brief Release Framebuffer ownership
     *
     * Called during swapchain cleanup. Notifies RenderTargetManager that this
     * Framebuffer should be destroyed.
     *
     * @param rtm RenderTargetManager instance
     * @param framebuffer Framebuffer to release
     */
    void RenderTargetManager_ReleaseFramebuffer(
        RenderTargetManager *rtm,
        Framebuffer *framebuffer);

    // ============================================
    // RenderPassManager Extensions
    // ============================================

    /**
     * @brief Acquire or create RenderPass for swapchain
     *
     * RenderPassManager manages a cache of RenderPass objects.
     * This method acquires or creates a RenderPass appropriate for swapchain rendering.
     *
     * @param rpm RenderPassManager instance
     * @param color_format Color attachment format
     * @param depth_format Depth attachment format (VK_FORMAT_UNDEFINED for none)
     * @param subpass_count Number of subpasses (default 2)
     * @return RenderPass* owned by RenderPassManager (reference counted)
     */
    RenderPass *RenderPassManager_AcquireSwapchainRenderPass(
        RenderPassManager *rpm,
        VkFormat color_format,
        VkFormat depth_format = VK_FORMAT_UNDEFINED,
        uint32_t subpass_count = 2);

    /**
     * @brief Release RenderPass reference
     *
     * Called when a swapchain frame no longer needs this RenderPass.
     * If reference count reaches zero, RenderPassManager may destroy it.
     *
     * @param rpm RenderPassManager instance
     * @param render_pass RenderPass to release
     */
    void RenderPassManager_ReleaseRenderPass(
        RenderPassManager *rpm,
        RenderPass *render_pass);

    // ============================================
    // Frame Resource Assembly Helper
    // ============================================

    /**
     * @brief Populate FrameResources structure from manager resources
     *
     * This helper function assembles a FrameResources structure by combining
     * resources obtained from different managers and the Device.
     *
     * Useful for validation and debugging to ensure all frame resources are properly set.
     *
     * @param frame FrameResources to populate
     * @param render_pass RenderPass (from RenderPassManager)
     * @param framebuffer Framebuffer (from RenderTargetManager)
     * @param image_view ImageView (from TextureManager)
     * @param vk_image VkImage (from Swapchain)
     * @return true if all required resources are set, false if any are null
     */
    bool ValidateFrameResources(const FrameResources &frame);

} // namespace ManagerExtensions

}//namespace hgl::graph

#endif // __VK_MANAGER_EXTENSIONS_H__
