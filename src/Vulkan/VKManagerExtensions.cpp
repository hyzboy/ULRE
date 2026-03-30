#include<hgl/vk/VKManagerExtensions.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>

namespace hgl::graph{

namespace ManagerExtensions
{

// ============================================
// Frame Resource Validation Helper
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
        // dynamic rendering: no framebuffer required
    }

    if (frame.render_pass == VK_NULL_HANDLE)
    {
        // dynamic rendering: no VkRenderPass required
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
