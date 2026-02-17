#ifndef __VK_FRAME_DATA_H__
#define __VK_FRAME_DATA_H__

#include<vulkan/vulkan.h>
#include<vector>

namespace hgl::graph
{
    /**
     * @struct FrameResources
     * @brief Pure data container for a single frame's resources
     * 
     * This structure contains ONLY REFERENCES to resources owned by various managers.
     * It does NOT own any resources and should never attempt to delete them.
     * 
     * Resource Ownership Model:
     * - VkImageView: owned by TextureManager
     * - VkFramebuffer: owned by RenderTargetManager
     * - VkRenderPass: owned by RenderPassManager
     * - VkCommandBuffer: owned by CommandPool (Device)
     * - VkQueue: owned by Device
     * - VkSemaphore: owned by SwapchainModule
     * - VkFence: owned by Device/SwapchainModule
     * - VkImage: owned by Swapchain
     * 
     * The only action this structure can perform is Clear(), which nullifies all pointers.
     */
    struct FrameResources
    {
        uint32_t frame_index = 0;                    ///< Current frame index (0-2 for triple buffering)
        
        // Semaphores for synchronization (owned by SwapchainModule)
        VkSemaphore image_acquired_semaphore = VK_NULL_HANDLE;      ///< Signal when swapchain image is ready
        VkSemaphore render_complete_semaphore = VK_NULL_HANDLE;     ///< Signal when rendering is complete
        
        // Swapchain image info (owned by Swapchain)
        VkImage vk_image = VK_NULL_HANDLE;           ///< Swapchain image
        uint32_t image_index = ~0u;                  ///< Index of swapchain image
        VkImageView image_view = VK_NULL_HANDLE;     ///< ImageView for rendering (owned by TextureManager)
        
        // Rendering resources (owned by respective managers)
        VkFramebuffer framebuffer = VK_NULL_HANDLE;  ///< Owned by RenderTargetManager
        VkRenderPass render_pass = VK_NULL_HANDLE;   ///< Owned by RenderPassManager
        
        // Command execution (owned by CommandPool/Device)
        VkCommandBuffer cmd_buffer = VK_NULL_HANDLE; ///< Command buffer from CommandPool
        VkQueue queue = VK_NULL_HANDLE;              ///< Graphics queue from Device
        
        // Synchronization fence (owned by Device/SwapchainModule)
        VkFence fence = VK_NULL_HANDLE;              ///< GPU-CPU synchronization fence
        
        // Additional resource references
        std::vector<VkImageView> texture_views;      ///< References to texture views used in rendering
        
        /**
         * @brief Clear all resource references without deleting them
         * 
         * This method should ONLY clear pointers to indicate the resources are no longer
         * valid for this frame. It does NOT delete anything.
         * 
         * Called when:
         * - Preparing frame for reuse
         * - During SwapchainModule shutdown (AFTER managers have released resources)
         */
        void Clear()
        {
            image_acquired_semaphore = VK_NULL_HANDLE;
            render_complete_semaphore = VK_NULL_HANDLE;
            vk_image = VK_NULL_HANDLE;
            image_index = ~0u;
            image_view = VK_NULL_HANDLE;
            framebuffer = VK_NULL_HANDLE;
            render_pass = VK_NULL_HANDLE;
            cmd_buffer = VK_NULL_HANDLE;
            queue = VK_NULL_HANDLE;
            fence = VK_NULL_HANDLE;
            texture_views.clear();
        }
    };

    /**
     * @brief Legacy typedef for backwards compatibility
     * Older code may reference FrameData, which is now FrameResources
     */
    using FrameData = FrameResources;

} // namespace hgl::graph

#endif // __VK_FRAME_DATA_H__
