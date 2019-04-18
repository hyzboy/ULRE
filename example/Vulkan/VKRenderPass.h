#ifndef HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class RenderPass
{
    VkDevice device;
    VkRenderPass render_pass;

public:

    RenderPass(VkDevice d,VkRenderPass rp)
    {
        device=d;
        render_pass=rp;
    }
    virtual ~RenderPass();

    operator VkRenderPass(){return render_pass;}
};//class RenderPass
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
