#ifndef __VK_FRAME_DATA_H__
#define __VK_FRAME_DATA_H__

#include<vulkan/vulkan.h>
#include<vector>
#include<hgl/vk/VKNamespace.h>

namespace hgl::graph{

// Forward declarations for wrapper classes
class DeviceQueue;
class Semaphore;
class Fence;

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
    Semaphore *image_acquired_semaphore = nullptr;      ///< Signal when swapchain image is ready
    Semaphore *render_complete_semaphore = nullptr;     ///< Signal when rendering is complete
    
    // Swapchain image info (owned by Swapchain)
    VkImage vk_image = VK_NULL_HANDLE;           ///< Swapchain image
    uint32_t image_index = ~0u;                  ///< Index of swapchain image
    VkImageView image_view = VK_NULL_HANDLE;     ///< ImageView for rendering (owned by TextureManager)
    
    // Rendering resources (owned by respective managers)
    VkFramebuffer framebuffer = VK_NULL_HANDLE;  ///< Owned by RenderTargetManager
    VkRenderPass render_pass = VK_NULL_HANDLE;   ///< Owned by RenderPassManager
    
    // Command execution (owned by SwapchainModule)
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE; ///< Command buffer from CommandPool
    DeviceQueue *queue = nullptr;                ///< Graphics queue wrapper (owned by SwapchainModule)
    
    // Synchronization fence (owned by SwapchainModule)
    Fence *fence = nullptr;                      ///< GPU-CPU synchronization fence
    
    // Additional resource references
    std::vector<VkImageView> texture_views;      ///< References to texture views used in rendering
    
    /**
     * @brief Clear all resource references
     * 
     * This method clears all pointers and DELETES the owned objects:
     * - Semaphore*: deletes the semaphore wrappers (per-frame)
     * - Fence*: deletes the fence wrapper (per-frame)
     * 
     * NOTE: DeviceQueue is NOT deleted here because it's shared across all frames
     * and managed by SwapchainData. The SwapchainData::Clear() method handles queue deletion.
     * 
     * Called when:
     * - Clearing individual frame references
     * - During SwapchainModule cleanup (via SwapchainData::Clear())
     */
    void Clear()
    {
        // Delete per-frame wrapper objects (not shared)
        if (image_acquired_semaphore) { delete image_acquired_semaphore; image_acquired_semaphore = nullptr; }
        if (render_complete_semaphore) { delete render_complete_semaphore; render_complete_semaphore = nullptr; }
        if (fence) { delete fence; fence = nullptr; }
        
        // NOTE: queue is NOT deleted here - it's shared and managed by SwapchainData
        
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

}//namespace hgl::graph

#endif // __VK_FRAME_DATA_H__
