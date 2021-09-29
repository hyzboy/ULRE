#ifndef HGL_VULKAN_RENDER_CONTEXT_INCLUDE
#define HGL_VULKAN_RENDER_CONTEXT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKSwapchain.h>
VK_NAMESPACE_BEGIN
/**
 * 渲染控制上下文
 */
class RenderContext
{
protected:

    GPUDevice *device;

    VkExtent2D extent;

public:

    RenderContext(GPUDevice *,const VkExtent2D &);
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
VK_NAMESPACE_END
#endif//HGL_VULKAN_RENDER_CONTEXT_INCLUDE
