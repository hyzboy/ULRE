#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN

void RenderTargetData::Clear()
{
    SAFE_CLEAR(queue);
    SAFE_CLEAR(render_complete_semaphore);
    SAFE_CLEAR(fbo);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(color_textures,color_count);
    SAFE_CLEAR(depth_texture);
}

VK_NAMESPACE_END