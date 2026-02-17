#ifndef __VK_SWAPCHAIN_DATA_H__
#define __VK_SWAPCHAIN_DATA_H__

#include<vulkan/vulkan.h>
#include<vector>
#include"./VKFrameData.h"

namespace hgl::graph
{
    /**
     * @struct SwapchainData
     * @brief Container for swapchain and its associated frame resources
     * 
     * This structure aggregates all data related to a swapchain and its frames.
     * It is owned by SwapchainModule and should not be accessed directly by other code.
     * 
     * Resource Ownership:
     * - VkSwapchain: owned by SwapchainModule (created via vkCreateSwapchainKHR)
     * - FrameResources: data container (no ownership, only contains references)
     * - VkSemaphore*: owned by SwapchainModule (created via vkCreateSemaphore)
     * - VkFence*: owned by SwapchainModule/Device (created via vkCreateFence)
     * 
     * Typical frame count: 2-3 (double/triple buffering)
     */
    struct SwapchainData
    {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;   ///< Vulkan swapchain handle
        std::vector<FrameResources> frames;           ///< Per-frame resources (typically 2-3 frames)
        
        uint32_t frame_count = 0;                     ///< Number of frames (2 or 3)
        uint32_t current_frame_index = 0;             ///< Current frame being rendered
        
        // Shared resources (all frames share these)
        DeviceQueue *shared_queue = nullptr;         ///< Shared graphics queue for all frames
        
        // Swapchain properties
        VkExtent2D extent = {0, 0};                   ///< Swapchain image extent
        VkFormat image_format = VK_FORMAT_UNDEFINED;  ///< Swapchain image format
        
        /**
         * @brief Get the current frame resources
         * @return Reference to current FrameResources
         */
        inline FrameResources& GetCurrentFrame()
        {
            return frames[current_frame_index];
        }

        /**
         * @brief Get the current frame resources (const)
         * @return Const reference to current FrameResources
         */
        inline const FrameResources& GetCurrentFrame() const
        {
            return frames[current_frame_index];
        }

        /**
         * @brief Get frame resources at specific index
         * @param index Frame index to retrieve
         * @return Reference to FrameResources at index
         */
        inline FrameResources& GetFrame(uint32_t index)
        {
            return frames[index];
        }

        /**
         * @brief Get frame resources at specific index (const)
         * @param index Frame index to retrieve
         * @return Const reference to FrameResources at index
         */
        inline const FrameResources& GetFrame(uint32_t index) const
        {
            return frames[index];
        }

        /**
         * @brief Advance to next frame in the swapchain chain
         * 
         * Should be called after presenting each frame. Wraps around from
         * frame_count-1 back to 0.
         */
        inline void AdvanceFrame()
        {
            current_frame_index = (current_frame_index + 1) % frame_count;
        }

        /**
         * @brief Clear all frame resources and delete shared resources
         * 
         * Called during swapchain destruction. Clears all frame references
         * and deletes the shared queue.
         */
        void Clear()
        {
            // Clear frame resources (deletes semaphores and fences, but NOT queue since it's shared)
            for (auto& frame : frames)
            {
                frame.Clear();
            }
            
            // Delete shared queue (owned by SwapchainData)
            if (shared_queue)
            {
                delete shared_queue;
                shared_queue = nullptr;
                
                // Clear frame.queue pointers since the shared queue is now deleted
                for (auto& frame : frames)
                {
                    frame.queue = nullptr;
                }
            }
            
            swapchain = VK_NULL_HANDLE;
            current_frame_index = 0;
        }
    };

} // namespace hgl::graph

#endif // __VK_SWAPCHAIN_DATA_H__
