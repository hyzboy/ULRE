#ifndef __VK_FRAME_DATA_H__
#define __VK_FRAME_DATA_H__

#include<vulkan/vulkan.h>
#include<vector>

VK_NAMESPACE_BEGIN
class DeviceQueue;
class Semaphore;
class Fence;
VK_NAMESPACE_END

namespace hgl::graph
{
    /**
     * @struct FrameResources
     * @brief Data container for a single frame's resources
     * 
     * This structure contains references to resources. Some are owned by various managers,
     * while DeviceQueue, Semaphore, and Fence are owned by SwapchainModule.
     * 
     * Resource Ownership Model:
     * - VkImageView: owned by TextureManager
     * - VkFramebuffer: owned by RenderTargetManager
     * - VkRenderPass: owned by RenderPassManager
     * - VkCommandBuffer: owned by CommandPool (Device)
     * - DeviceQueue*: owned by SwapchainModule (for queue operations)
     * - Semaphore*: owned by SwapchainModule (for synchronization)
     * - Fence*: owned by SwapchainModule (for GPU-CPU sync)
     * - VkImage: owned by Swapchain
     * 
     * Cleanup is managed by SwapchainModule::DestroyPerFrameResources()
     */
    struct FrameResources
    {
        uint32_t frame_index = 0;                    ///< Current frame index (0-2 for triple buffering)
        
        // Semaphores for synchronization (owned by SwapchainModule)
        hgl::vk::Semaphore *image_acquired_semaphore = nullptr;      ///< Signal when swapchain image is ready
        hgl::vk::Semaphore *render_complete_semaphore = nullptr;     ///< Signal when rendering is complete
        
        // Swapchain image info (owned by Swapchain)
        VkImage vk_image = VK_NULL_HANDLE;           ///< Swapchain image
        uint32_t image_index = ~0u;                  ///< Index of swapchain image
        VkImageView image_view = VK_NULL_HANDLE;     ///< ImageView for rendering (owned by TextureManager)
        
        // Rendering resources (owned by respective managers)
        VkFramebuffer framebuffer = VK_NULL_HANDLE;  ///< Owned by RenderTargetManager
        VkRenderPass render_pass = VK_NULL_HANDLE;   ///< Owned by RenderPassManager
        
        // Command execution (owned by SwapchainModule)
        VkCommandBuffer cmd_buffer = VK_NULL_HANDLE; ///< Command buffer from CommandPool
        hgl::vk::DeviceQueue *queue = nullptr;       ///< Graphics queue wrapper (owned by SwapchainModule)
        
        // Synchronization fence (owned by SwapchainModule)
        hgl::vk::Fence *fence = nullptr;             ///< GPU-CPU synchronization fence
        
        // Additional resource references
        std::vector<VkImageView> texture_views;      ///< References to texture views used in rendering
        
        /**
         * @brief Clear all resource references and delete owned objects
         * 
         * This method clears all pointers and DELETES the wrapper objects owned by SwapchainModule:
         * - DeviceQueue*: deletes the queue wrapper
         * - Semaphore*: deletes the semaphore wrappers
         * - Fence*: deletes the fence wrapper
         * 
         * Called when:
         * - Preparing frame for reuse
         * - During SwapchainModule::DestroyPerFrameResources()
         */
        void Clear()
        {
            // Delete wrapper objects owned by SwapchainModule
            if (queue) { delete queue; queue = nullptr; }
            if (image_acquired_semaphore) { delete image_acquired_semaphore; image_acquired_semaphore = nullptr; }
            if (render_complete_semaphore) { delete render_complete_semaphore; render_complete_semaphore = nullptr; }
            if (fence) { delete fence; fence = nullptr; }
            
            // Clear other handles (owned by other managers)
            vk_image = VK_NULL_HANDLE;
            image_index = ~0u;
            image_view = VK_NULL_HANDLE;
            framebuffer = VK_NULL_HANDLE;
            render_pass = VK_NULL_HANDLE;
            cmd_buffer = VK_NULL_HANDLE;
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
