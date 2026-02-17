#ifndef HGL_VULKAN_RENDER_CONTEXT_INCLUDE
#define HGL_VULKAN_RENDER_CONTEXT_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/vk/VKSwapchain.h>
namespace hgl::graph{
/**
 * 渲染控制上下文
 */
class RenderContext
{
protected:

    VulkanDevice *device;

    VkExtent2D extent;

public:

    RenderContext(VulkanDevice *,const VkExtent2D &);
    virtual ~RenderContext();

    void Prepare(
};//class RenderContext

class RenderContextSwapchain:public RenderContext
{
    Swapchain *swapchain;

public:

    void RequestPresentMode(const VkPresentModeKHR present_mode);
    void RequestImageFormat(const VkFormat format);
};//class RenderContextSwapchain:public RenderContext
}//namespace hgl::graph
#endif//HGL_VULKAN_RENDER_CONTEXT_INCLUDE
