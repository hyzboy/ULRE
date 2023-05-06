#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
RenderCmdBuffer::RenderCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb):GPUCmdBuffer(attr,cb)
{
    cv_count=0;
    clear_values=nullptr;

    hgl_zero(render_area);
    hgl_zero(viewport);

    default_line_width=1.0;

    fbo=nullptr;
    pipeline_layout=VK_NULL_HANDLE;
}

RenderCmdBuffer::~RenderCmdBuffer()
{
    if(clear_values)
        hgl_free(clear_values);
}

void RenderCmdBuffer::SetFBO(Framebuffer *fb)
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

void RenderCmdBuffer::SetRenderArea(const VkExtent2D &ext2d)
{
    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext2d;
}

bool RenderCmdBuffer::BindFramebuffer(RenderPass *rp,Framebuffer *fb)
{
    if(!rp||!fb)return(false);

    SetFBO(fb);

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=fb->GetExtent();

    rp_begin.renderPass         = rp->GetVkRenderPass();
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

bool RenderCmdBuffer::BeginRenderPass()
{
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);
    vkCmdSetLineWidth(cmd_buf,default_line_width);

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

bool RenderCmdBuffer::BindDescriptorSets(Renderable *ri)
{
    if(!ri)return(false);

    {
        uint32_t count=0;

        MaterialParameters *mp;
        VkDescriptorSet ds[DESCRIPTOR_SET_TYPE_COUNT];

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            mp=ri->GetMP((DescriptorSetType)i);

            if(mp)
            {
                ds[count]=mp->GetVkDescriptorSet();
                ++count;
            }
        }

        if(count>0)
        {
            pipeline_layout=ri->GetPipelineLayout();

            vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,count,ds,0,0);
        }
    }

    return(true);
}

void RenderCmdBuffer::BindIBO(const IndexBufferData *ibd)
{
    vkCmdBindIndexBuffer(   cmd_buf,
                            ibd->buffer->GetBuffer(),
                            ibd->offset,
                            VkIndexType(ibd->buffer->GetType()));
}

bool RenderCmdBuffer::BindVBO(Renderable *ri)
{
    if(!ri)
        return(false);

    const VertexInputData *vid=ri->GetVertexInputData();

    if(vid->binding_count<=0)
        return(false);

    vkCmdBindVertexBuffers(cmd_buf,0,vid->binding_count,vid->buffer_list,vid->buffer_offset);

    IndexBuffer *indices_buffer=vid->index_buffer->buffer;

    if(indices_buffer)
        vkCmdBindIndexBuffer(cmd_buf,indices_buffer->GetBuffer(),vid->index_buffer->offset,VkIndexType(indices_buffer->GetType()));

    return(true);
}

void RenderCmdBuffer::DrawIndirect( VkBuffer        buffer,
                                    VkDeviceSize    offset,
                                    uint32_t        drawCount,
                                    uint32_t        stride)
{
    if(this->dev_attr->physical_device->SupportMDI())
        vkCmdDrawIndirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        vkCmdDrawIndirect(cmd_buf,buffer,offset+i*stride,1,stride);
}

void RenderCmdBuffer::DrawIndexedIndirect(  VkBuffer        buffer,
                                            VkDeviceSize    offset,
                                            uint32_t        drawCount,
                                            uint32_t        stride)
{
    if(this->dev_attr->physical_device->SupportMDI())
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset+i*stride,1,stride);
}
VK_NAMESPACE_END
