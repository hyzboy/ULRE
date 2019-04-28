#include"VKCommandBuffer.h"
#include"VKRenderPass.h"
#include"VKFramebuffer.h"
#include"VKPipeline.h"
#include"VKBuffer.h"
#include"VKRenderable.h"
#include"VKDescriptorSets.h"

VK_NAMESPACE_BEGIN
CommandBuffer::CommandBuffer(VkDevice dev,const VkExtent2D &extent,VkCommandPool cp,VkCommandBuffer cb)
{
    device=dev;
    pool=cp;
    cmd_buf=cb;

    clear_values[0].color.float32[0] = 0.2f;
    clear_values[0].color.float32[1] = 0.2f;
    clear_values[0].color.float32[2] = 0.2f;
    clear_values[0].color.float32[3] = 0.2f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=extent;
}

CommandBuffer::~CommandBuffer()
{
    VkCommandBuffer cmd_bufs[1] = {cmd_buf};
    vkFreeCommandBuffers(device, pool, 1, cmd_bufs);
}

bool CommandBuffer::Begin(RenderPass *rp,Framebuffer *fb)
{
    VkCommandBufferBeginInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = nullptr;
    cmd_buf_info.flags = 0;
    cmd_buf_info.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmd_buf, &cmd_buf_info)!=VK_SUCCESS)
        return(false);

    VkRenderPassBeginInfo rp_begin;

    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = nullptr;
    rp_begin.renderPass = *rp;
    rp_begin.framebuffer = *fb;
    rp_begin.renderArea = render_area;
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    viewport.x=0;
    viewport.y=0;
    viewport.minDepth=0.0f;
    viewport.maxDepth=1.0f;
    viewport.width=render_area.extent.width;
    viewport.height=render_area.extent.height;

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);

    return(true);
}

bool CommandBuffer::Bind(Pipeline *p)
{
    if(!p)return(false);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,*p);

    if(p->GetDescriptorSetCount()>0)
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, p->GetLayout(), 0, p->GetDescriptorSetCount(),p->GetDescriptorSets(), 0, nullptr);

    return(true);
}

bool CommandBuffer::Bind(Renderable *render_obj)
{
    if(!render_obj)
        return(false);

    const uint count=render_obj->GetBufferCount();

    if(count<=0)
        return(false);

    vkCmdBindVertexBuffers(cmd_buf,0,count,render_obj->GetBuffer(),render_obj->GetOffset());

    IndexBuffer *indices_buffer=render_obj->GetIndexBuffer();

    if(indices_buffer)    
        vkCmdBindIndexBuffer(cmd_buf,*indices_buffer,render_obj->GetIndexOffset(),indices_buffer->GetType());

    return(true);
}
void CommandBuffer::SetDepthBias(float constant_factor,float clamp,float slope_factor)
{
    vkCmdSetDepthBias(cmd_buf,constant_factor,clamp,slope_factor);
}

void CommandBuffer::SetDepthBounds(float min_db,float max_db)
{
    vkCmdSetDepthBounds(cmd_buf,min_db,max_db);
}

void CommandBuffer::SetStencilCompareMask(VkStencilFaceFlags faceMask,uint32_t compareMask)
{
    vkCmdSetStencilCompareMask(cmd_buf,faceMask,compareMask);
}

void CommandBuffer::SetStencilWriteMask(VkStencilFaceFlags faceMask,uint32_t compareMask)
{
    vkCmdSetStencilWriteMask(cmd_buf,faceMask,compareMask);
}

void CommandBuffer::SetStencilReference(VkStencilFaceFlags faceMask,uint32_t compareMask)
{
    vkCmdSetStencilReference(cmd_buf,faceMask,compareMask);
}

void CommandBuffer::SetBlendConstants(const float constants[4])
{
    vkCmdSetBlendConstants(cmd_buf,constants);
}

void CommandBuffer::SetLineWidth(float line_width)
{
    vkCmdSetLineWidth(cmd_buf,line_width);
}
 
void CommandBuffer::Draw(const uint32_t vertex_count)
{
    vkCmdDraw(cmd_buf,vertex_count,1,0,0);
}

void CommandBuffer::Draw(const uint32_t vertex_count,const uint32_t instance_count,const uint32_t first_vertex,const uint32_t first_instance)
{
    vkCmdDraw(cmd_buf,vertex_count,instance_count,first_vertex,first_instance);
}

void CommandBuffer::DrawIndexed(const uint32_t index_count)
{
    vkCmdDrawIndexed(cmd_buf,index_count,1,0,0,0);
}

void CommandBuffer::DrawIndexed(const uint32_t index_count,const uint32_t instance_count,const uint32_t first_index,const uint32_t vertex_offset,const uint32_t first_instance)
{
    vkCmdDrawIndexed(cmd_buf,index_count,instance_count,first_index,vertex_offset,first_instance);
}

bool CommandBuffer::End()
{
    vkCmdEndRenderPass(cmd_buf);
    return(vkEndCommandBuffer(cmd_buf)==VK_SUCCESS);
}
VK_NAMESPACE_END
