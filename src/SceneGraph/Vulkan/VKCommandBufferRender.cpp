#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKRenderTarget.h>

VK_NAMESPACE_BEGIN
RenderCmdBuffer::RenderCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb):VulkanCmdBuffer(attr,cb)
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

bool RenderCmdBuffer::SetDescriptorBinding(DescriptorBinding *db)
{
    if(!db)
        return(false);

    const int index=int(db->GetType())-int(DescriptorSetType::BEGIN_RANGE);

    desc_binding[index]=db;

    return(true);
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

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        if(desc_binding[i])
            desc_binding[i]->Bind(mtl);
    }

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

bool RenderCmdBuffer::BindDataBuffer(const MeshDataBuffer *mesh_data_buffer)
{
    if(!mesh_data_buffer)
        return(false);

    if(mesh_data_buffer->vab_count<=0)
        return(false);

    vkCmdBindVertexBuffers(cmd_buf,
                           0,               //first binding
                           mesh_data_buffer->vab_count,
                           mesh_data_buffer->vab_list,
                           mesh_data_buffer->vab_offset);        //vab byte offsets

    if(mesh_data_buffer->ibo)
        BindIBO(mesh_data_buffer->ibo);

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

void RenderCmdBuffer::Draw(const MeshDataBuffer *mesh_data_buffer,const MeshRenderData *mesh_render_data,const uint32_t instance_count,const uint32_t first_instance)
{
    if(!mesh_data_buffer||!mesh_render_data)
        return;

    if (mesh_data_buffer->ibo)
        vkCmdDrawIndexed(   cmd_buf,
                            mesh_render_data->index_count,
                            instance_count,
                            mesh_render_data->first_index,
                            mesh_render_data->vertex_offset, //这里的vertexOffset是针对所有VAB的
                            first_instance);    //这里的first_instance针对的是instance Rate更新的VAB的起始实例数，不是指instance批量渲染
    else
        vkCmdDraw(          cmd_buf,
                            mesh_render_data->vertex_count,
                            instance_count,
                            mesh_render_data->vertex_offset,
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
