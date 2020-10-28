#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKRenderableInstance.h>

VK_NAMESPACE_BEGIN
RenderCommand::RenderCommand(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb):GPUCmdBuffer(dev,cp,cb)
{
    cv_count=0;
    clear_values=nullptr;

    hgl_zero(render_area);
    hgl_zero(viewport);

    default_line_width=1.0;

    fbo=nullptr;
    pipeline_layout=VK_NULL_HANDLE;
}

RenderCommand::~RenderCommand()
{
    if(clear_values)
        hgl_free(clear_values);
}

void RenderCommand::SetFBO(Framebuffer *fb)
{
    if(fbo==fb)return;

    fbo=fb;
    cv_count=fbo->GetAttachmentCount();

    if(cv_count>0)
    {
        clear_values=hgl_align_realloc<VkClearValue>(clear_values,cv_count);

        clear_values[cv_count-1].depthStencil.depth = 1.0f;
        clear_values[cv_count-1].depthStencil.stencil = 0;
    }
    else
    {
        if(clear_values)
        {
            hgl_free(clear_values);
            clear_values=nullptr;
        }
    }

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent.width=0;
    render_area.extent.height=0;
}

void RenderCommand::SetRenderArea(const VkExtent2D &ext2d)
{
    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext2d;
}

bool RenderCommand::BindFramebuffer(RenderPass *rp,Framebuffer *fb)
{
    if(!rp||!fb)return(false);

    SetFBO(fb);

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=fb->GetExtent();

    rp_begin.renderPass         = *rp;
    rp_begin.framebuffer        = *fb;
    rp_begin.renderArea         = render_area;
    rp_begin.clearValueCount    = cv_count;
    rp_begin.pClearValues       = clear_values;

    viewport.x          = 0;
    viewport.y          = 0;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;
    viewport.width      = render_area.extent.width;
    viewport.height     = render_area.extent.height;

    return(true);
};

bool RenderCommand::BeginRenderpass()
{
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);
    vkCmdSetLineWidth(cmd_buf,default_line_width);

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

bool RenderCommand::BindVAB(RenderableInstance *ri)
{
    if(!ri)
        return(false);

    const uint count=ri->GetBufferCount();

    if(count<=0)
        return(false);

    vkCmdBindVertexBuffers(cmd_buf,0,count,ri->GetBuffer(),ri->GetBufferSize());

    IndexBuffer *indices_buffer=ri->GetIndexBuffer();

    if(indices_buffer)
        vkCmdBindIndexBuffer(cmd_buf,indices_buffer->GetBuffer(),ri->GetIndexBufferOffset(),VkIndexType(indices_buffer->GetType()));

    return(true);
}
VK_NAMESPACE_END
