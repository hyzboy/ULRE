#include<hgl/vk/VKRenderTargetLegacy.h>
#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKFrameData.h>
#include<hgl/vk/VKSwapchainData.h>
#include<hgl/Macro.h>

VK_NAMESPACE_BEGIN

namespace
{
    /**
     * @brief Wrapper class to convert FrameResources to RenderTargetData format
     * 
     * This internal class allows old code expecting RenderTargetData to work with
     * the new FrameResources structure. It acts as a compatibility adapter.
     */
    class FrameResourcesWrapper : public RenderTargetData
    {
    private:
        FrameResources *frame;  ///< Reference to the actual FrameResources

    public:
        FrameResourcesWrapper(FrameResources &f) : frame(&f)
        {
            // Initialize RenderTargetData with references from FrameResources
            fbo = frame->framebuffer;
            queue = frame->queue;
            render_complete_semaphore = frame->render_complete_semaphore;
            cmd_buf = frame->cmd_buffer;
            
            // Note: color_count is set to 1 for swapchain (single color image)
            color_count = 1;
            
            // Set up color texture references if available
            // TODO: This needs the actual Texture2D* reference
            // For now, we'll leave this null - it should be set by SwapchainModule
            
            depth_texture = nullptr;
        }

        virtual ~FrameResourcesWrapper()
        {
            // DO NOT delete owned resources - just clear references
            // The frame resources are still owned by managers
            Clear();
        }

        virtual void Clear()
        {
            // Clear references without deleting
            fbo = VK_NULL_HANDLE;
            queue = VK_NULL_HANDLE;
            render_complete_semaphore = VK_NULL_HANDLE;
            cmd_buf = VK_NULL_HANDLE;
            
            if (color_textures)
            {
                SAFE_DELETE_ARRAY(color_textures);
            }
            
            depth_texture = nullptr;
            color_count = 0;
        }
    };
}

// ============================================
// RenderTargetLegacy Implementation
// ============================================

void *RenderTargetLegacy::FrameResourcesToRenderTargetData(FrameResources &frame)
{
    // Create a wrapper that presents FrameResources as RenderTargetData
    FrameResourcesWrapper *wrapper = new FrameResourcesWrapper(frame);
    return static_cast<void *>(wrapper);
}

void *RenderTargetLegacy::GetLegacyFrameData(SwapchainData &swapchain_data)
{
    if (swapchain_data.frames.empty())
    {
        return nullptr;
    }

    // Convert current frame to legacy format
    FrameResources &current_frame = swapchain_data.GetCurrentFrame();
    return FrameResourcesToRenderTargetData(current_frame);
}

VK_NAMESPACE_END
