#include<hgl/graph/vulkan/VKRenderTarget.h>

VK_NAMESPACE_BEGIN
RenderTarget::~RenderTarget()
{
    if(fb)
        delete fb;
}
VK_NAMESPACE_END
