#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN
RenderTarget::RenderTarget(DeviceQueue *q,Semaphore *s)
{
    queue=q;
    render_pass=nullptr;
    fbo=nullptr;

    color_count=0;
    color_textures=nullptr;
    depth_texture=nullptr;
    render_complete_semaphore=s;
}

RenderTarget::RenderTarget(DeviceQueue *q,Semaphore *s,RenderPass *_rp,Framebuffer *_fb,Texture2D **ctl,const uint32_t cc,Texture2D *dt)
{
    queue=q;
    render_pass=_rp;
    fbo=_fb;
    
    depth_texture=dt;

    color_count=cc;
    if(color_count>0)
    {
        color_textures=new Texture2D *[color_count];
        hgl_cpy(color_textures,ctl,color_count);

        extent.width=color_textures[0]->GetWidth();
        extent.height=color_textures[0]->GetHeight();
    }
    else
    {
        color_textures=nullptr;

        if(depth_texture)
        {
            extent.width=depth_texture->GetWidth();
            extent.height=depth_texture->GetHeight();
        }
    }

    render_complete_semaphore=s;
}

RenderTarget::~RenderTarget()
{
    SAFE_CLEAR(queue);
    SAFE_CLEAR(depth_texture);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(color_textures,color_count);
    
    SAFE_CLEAR(render_complete_semaphore);
    SAFE_CLEAR(fbo);
}

bool RenderTarget::Submit(RenderCmdBuffer *command_buffer,Semaphore *present_complete_semaphore)
{
    return queue->Submit(*command_buffer,present_complete_semaphore,render_complete_semaphore);
}
VK_NAMESPACE_END