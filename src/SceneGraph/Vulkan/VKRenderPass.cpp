#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKDevice.h>
VK_NAMESPACE_BEGIN
RenderPass::~RenderPass()
{
    vkDestroyRenderPass(device,render_pass,nullptr);
}
VK_NAMESPACE_END
