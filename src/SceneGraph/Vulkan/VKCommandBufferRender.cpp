#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKRenderTarget.h>

VK_NAMESPACE_BEGIN
RenderCmdBuffer::RenderCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb):GPUCmdBuffer(attr,cb)
{
    cv_count=0;
    clear_values=nullptr;

    hgl_zero(render_area);
    hgl_zero(viewport);

    pipeline_layout=VK_NULL_HANDLE;
}

RenderCmdBuffer::~RenderCmdBuffer()
{
    if(clear_values)
        hgl_free(clear_values);
}

void RenderCmdBuffer::SetClear()
{
    if(cv_count>0)
    {
        clear_values=hgl_align_realloc<VkClearValue>(clear_values,cv_count);

        clear_values[cv_count-1].depthStencil.depth = 1.0f;
        clear_values[cv_count-1].depthStencil.stencil = 0;
    }
    else if(clear_values)
    {
        hgl_free(clear_values);
        clear_values=nullptr;
    }
}

void RenderCmdBuffer::SetRenderArea(const VkExtent2D &ext2d)
{
    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext2d;
}

bool RenderCmdBuffer::BindFramebuffer(Framebuffer *fbo)
{
    if(!fbo)return(false);

    cv_count=fbo->GetAttachmentCount();
    SetClear();

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=fbo->GetExtent();

    rp_begin.renderPass         = *fbo->GetRenderPass();
    rp_begin.framebuffer        = *fbo;
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

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

bool RenderCmdBuffer::BindDescriptorSets(Material *mtl)
{
    if(!mtl)return(false);

    {
        uint32_t count=0;

        MaterialParameters *mp;
        VkDescriptorSet ds[DESCRIPTOR_SET_TYPE_COUNT];

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            mp=mtl->GetMP((DescriptorSetType)i);

            if(mp)
            {
                mp->Update();

                ds[count]=mp->GetVkDescriptorSet();
                ++count;
            }
        }

        if(count>0)
        {
            pipeline_layout=mtl->GetPipelineLayout();

            vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,count,ds,0,0);
        }
    }

    return(true);
}

void RenderCmdBuffer::BindIBO(IndexBuffer *ibo,const VkDeviceSize byte_offset)
{
    if(!ibo)return;
    
    vkCmdBindIndexBuffer(cmd_buf,
                         ibo->GetBuffer(),
                         byte_offset,
                         VkIndexType(ibo->GetType()));
}

bool RenderCmdBuffer::BindDataBuffer(const PrimitiveDataBuffer *pdb)
{
    if(!pdb)
        return(false);

    if(pdb->vab_count<=0)
        return(false);

    vkCmdBindVertexBuffers(cmd_buf,
                           0,               //first binding
                           pdb->vab_count,
                           pdb->vab_list,
                           pdb->vab_offset);        //vab byte offsets

    if(pdb->ibo)
        BindIBO(pdb->ibo);

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

void RenderCmdBuffer::Draw(const PrimitiveDataBuffer *pdb,const PrimitiveRenderData *prd,const uint32_t instance_count,const uint32_t first_instance)
{
    if(!pdb||!prd)
        return;

    if (pdb->ibo)
        vkCmdDrawIndexed(   cmd_buf,
                            prd->index_count,
                            instance_count,
                            prd->first_index,
                            prd->vertex_offset, //这里的vertexOffset是针对所有VAB的
                            first_instance);    //这里的first_instance针对的是instance Rate更新的VAB的起始实例数，不是指instance批量渲染
    else
        vkCmdDraw(          cmd_buf,
                            prd->vertex_count,
                            instance_count,
                            prd->vertex_offset,
                            first_instance);
}

//void RenderCmdBuffer::DrawIndexed(const IBAccess *iba,const uint32_t instance_count)
//{
//    if(!iba||instance_count<=0)return;
//
//    vkCmdBindIndexBuffer(cmd_buf,
//                         iba->buffer->GetBuffer(),
//                         iba->start*iba->buffer->GetStride(),
//                         VkIndexType(iba->buffer->GetType()));
//
//    vkCmdDrawIndexed(cmd_buf,
//                     iba->count,
//                     instance_count,
//                     0,                 //first index
//                     0,                 //vertex offset
//                     0);                //first instance
//}
VK_NAMESPACE_END
