#ifndef __VK_RENDER_TARGET_LEGACY_H__
#define __VK_RENDER_TARGET_LEGACY_H__

#include<vulkan/vulkan.h>
#include"./VKFrameData.h"

namespace hgl::graph
{
    /**
     * @class RenderTargetLegacy
     * @brief Compatibility wrapper for converting between new and old architectures
     *
     * This class helps bridge the gap between the old RenderTargetData-based code
     * and the new SwapchainModule/FrameResources architecture.
     *
     * It provides conversion utilities and compatibility functions to allow
     * gradual migration from the old architecture to the new one.
     */
    class RenderTargetLegacy
    {
    public:
        /**
         * @brief Convert FrameResources to legacy RenderTargetData format
         *
         * This function creates a temporary wrapper around FrameResources
         * to provide compatibility with code expecting the old RenderTargetData structure.
         *
         * @param frame The new FrameResources to wrap
         * @return Pointer to wrapper (caller should not delete)
         */
        static void* FrameResourcesToRenderTargetData(FrameResources& frame);

        /**
         * @brief Get legacy frame data for current frame in swapchain
         *
         * Compatibility function to get frame data in the old format.
         * This allows old rendering code to continue working during migration.
         *
         * @param swapchain_data The new SwapchainData
         * @return Pointer to wrapped FrameResources
         */
        static void* GetLegacyFrameData(struct SwapchainData& swapchain_data);

    private:
        RenderTargetLegacy() = delete;  ///< Static utility class, no instantiation
    };

} // namespace hgl::graph

#endif // __VK_RENDER_TARGET_LEGACY_H__
