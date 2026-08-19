#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::graph{
bool RenderCmdBuffer::BindVAB(const VABList *vab_list)
{
    if(!vab_list)
    {
        LogError("VABList is null");
        return(false);
    }

    if(!vab_list->IsFull())
    {
        LogError("VABList is not full");
        return(false);
    }

    vkCmdBindVertexBuffers(cmd_buf,
                           0,                           //first binding
                           vab_list->GetWriteCount(),   //binding count (use actual count, not capacity)
                           vab_list->GetVABList(),      //buffers
                           vab_list->GetVABOffset());   //buffer offsets

    return(true);
}

RenderCmdBuffer::RenderCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb):VulkanCmdBuffer(attr,cb)
{
    cv_count=0;
    clear_values=nullptr;

    mem_zero(render_area);
    mem_zero(viewport);

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

        clear_values[cv_count-1].depthStencil.depth = 0.0f;
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

bool RenderCmdBuffer::BindDescriptorSets(ShaderProgram *mtl)
{
    if(!mtl)return(false);

    pipeline_layout=mtl->GetPipelineLayout();

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        MaterialParameters *mp=mtl->GetMP((DescriptorSetType)i);
        if(!mp)
            continue;

        mp->Update();

        const VkDescriptorSet ds=mp->GetVkDescriptorSet();
        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,uint32_t(i),1,&ds,0,nullptr);
    }

    return(true);
}
void RenderCmdBuffer::BindIBO(IndexBuffer *ibo,const VkDeviceSize byte_offset)
{
    //LogVerbose(u"BindIBO entry");

    if(!ibo)
    {
        LogError("Null IBO");
        return;
    }

    //std::cerr << "[RenderCmdBuffer::BindIBO] IBO buffer: " << ibo->GetBuffer() << std::endl;
    //std::cerr << "[RenderCmdBuffer::BindIBO] IBO type: " << (int)ibo->GetType() << std::endl;
    //std::cerr << "[RenderCmdBuffer::BindIBO] Byte offset: " << byte_offset << std::endl;

    vkCmdBindIndexBuffer(cmd_buf,
                         ibo->GetVkBuffer(),
                         byte_offset,
                         VkIndexType(ibo->GetType()));

//    std::cerr << "[RenderCmdBuffer::BindIBO] === EXIT ===" << std::endl;
}

bool RenderCmdBuffer::BindDataBuffer(const GeometryDataBuffer *geom_data_buffer)
{
    //LogVerbose(u"BindDataBuffer entry");

    if(!geom_data_buffer)
    {
        LogError("Null geometry data buffer");
        return(false);
    }

    if(geom_data_buffer->vab_count<=0)
    {
        // SSBO 顶点输入模式：管线无 VBO attribute（VIL 空）——顶点数据经
        // 顶点 SSBO 读取，无需 vkCmdBindVertexBuffers——无 VAB 是合法常态，
        // 返回 true（无绑定操作即成功）。VBO 模式 VIF 非空不会走到此分支。
        return(true);
    }

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] Calling vkCmdBindVertexBuffers..." << std::endl;

    // Log each buffer
    //for(uint32_t i = 0; i < geom_data_buffer->vab_count; i++)
    //{
    //    std::cerr << "[RenderCmdBuffer::BindDataBuffer]   Buffer[" << i << "]: "
    //              << geom_data_buffer->vab_list[i]
    //              << ", offset: " << geom_data_buffer->vab_offset[i] << std::endl;
    //}

    vkCmdBindVertexBuffers(cmd_buf,
                           0,               //first binding
                           geom_data_buffer->vab_count,
                           geom_data_buffer->vab_list,
                           geom_data_buffer->vab_offset);        //vab byte offsets

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] Vertex buffers bound" << std::endl;

    if(geom_data_buffer->ibo)
    {
//        std::cerr << "[RenderCmdBuffer::BindDataBuffer] Binding IBO: " << geom_data_buffer->ibo << std::endl;
        BindIBO(geom_data_buffer->ibo);
    }
    else
    {
//        std::cerr << "[RenderCmdBuffer::BindDataBuffer] No IBO to bind" << std::endl;
    }

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] === EXIT (success) ===" << std::endl;
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

void RenderCmdBuffer::Draw(const GeometryDataBuffer *geom_data_buffer,const GeometryDrawRange *geom_draw_range,const uint32_t instance_count,const uint32_t first_instance)
{
    //LogVerbose(u"Draw entry");

    if(!geom_data_buffer||!geom_draw_range)
    {
        LogError("Null parameter in Draw");
        return;
    }

    if (geom_data_buffer->ibo)
    {
        //std::cerr << "[RenderCmdBuffer::Draw] Using INDEXED draw" << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   index_count: " << geom_draw_range->index_count << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   first_index: " << geom_draw_range->first_index << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_offset: " << geom_draw_range->vertex_offset << std::endl;

        vkCmdDrawIndexed(   cmd_buf,
                            geom_draw_range->index_count,
                            instance_count,
                            geom_draw_range->first_index,
                            geom_draw_range->vertex_offset, //这里的vertexOffset是针对所有VAB的
                            first_instance);    //这里的first_instance针对的是instance Rate更新的VAB的起始实例数，不是指instance批量渲染

//        std::cerr << "[RenderCmdBuffer::Draw] vkCmdDrawIndexed called" << std::endl;
    }
    else
    {
        //std::cerr << "[RenderCmdBuffer::Draw] Using NON-INDEXED draw" << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_count: " << geom_draw_range->vertex_count << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_offset: " << geom_draw_range->vertex_offset << std::endl;

        vkCmdDraw(          cmd_buf,
                            geom_draw_range->vertex_count,
                            instance_count,
                            geom_draw_range->vertex_offset,
                            first_instance);

//        std::cerr << "[RenderCmdBuffer::Draw] vkCmdDraw called" << std::endl;
    }

//    std::cerr << "[RenderCmdBuffer::Draw] === EXIT ===" << std::endl;
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
}//namespace hgl::graph


