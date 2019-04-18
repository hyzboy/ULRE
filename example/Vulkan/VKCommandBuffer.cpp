#include"VKCommandBuffer.h"
#include"VKRenderPass.h"
#include"VKFramebuffer.h"
#include"VKPipeline.h"
#include"VKVertexInput.h"

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

bool CommandBuffer::Bind(RenderPass *rp,Framebuffer *fb,Pipeline *p)
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
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *p);

    return(true);
}

bool CommandBuffer::Bind(VertexInput *vi)
{
    if(!vi)
        return(false);

    const List<VkBuffer> &buf_list=vi->GetBufferList();

    if(buf_list.GetCount()<=0)
        return(false);

    constexpr VkDeviceSize zero_offsets[1]={0};

    vkCmdBindVertexBuffers(cmd_buf,0,buf_list.GetCount(),buf_list.GetData(),zero_offsets);

    return(true);
}
VK_NAMESPACE_END
