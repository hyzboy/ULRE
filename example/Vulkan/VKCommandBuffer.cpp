#include"VKCommandBuffer.h"
#include"VKRenderPass.h"
#include"VKFramebuffer.h"
#include"VKPipeline.h"
#include"VKPipelineLayout.h"
#include"VKVertexInput.h"
#include"VKDescriptorSets.h"

VK_NAMESPACE_BEGIN
CommandBuffer::CommandBuffer(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb)
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
}

CommandBuffer::~CommandBuffer()
{
    VkCommandBuffer cmd_bufs[1] = {cmd_buf};
    vkFreeCommandBuffers(device, pool, 1, cmd_bufs);
}

bool CommandBuffer::Begin(RenderPass *rp,Framebuffer *fb)
{
    VkRenderPassBeginInfo rp_begin;

    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = nullptr;
    rp_begin.renderPass = *rp;
    rp_begin.framebuffer = *fb;
    rp_begin.renderArea = render_area;
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    return(true);
}

bool CommandBuffer::Bind(Pipeline *p)
{
    if(!p)return(false);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *p);
    return(true);
}

bool CommandBuffer::Bind(PipelineLayout *pl)
{
    if(!pl)return(false);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pl, 0, pl->GetDescriptorSetCount(),pl->GetDescriptorSets(), 0, nullptr);
    return(true);
}

bool CommandBuffer::Bind(VertexInput *vi,const VkDeviceSize offset)
{
    if(!vi)
        return(false);

    const List<VkBuffer> &buf_list=vi->GetBufferList();

    if(buf_list.GetCount()<=0)
        return(false);

    VkDeviceSize zero_offsets[1]={offset};

    vkCmdBindVertexBuffers(cmd_buf,0,buf_list.GetCount(),buf_list.GetData(),zero_offsets);
    return(true);
}

void CommandBuffer::Draw(const uint32_t vertex_count)
{
    vkCmdDraw(cmd_buf,vertex_count,1,0,0);
}

void CommandBuffer::Draw(const uint32_t vertex_count,const uint32_t instance_count,const uint32_t first_vertex,const uint32_t first_instance)
{
    vkCmdDraw(cmd_buf,vertex_count,instance_count,first_vertex,first_instance);
}

void CommandBuffer::End()
{
    vkCmdEndRenderPass(cmd_buf);
}
VK_NAMESPACE_END
