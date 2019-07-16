#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
CommandBuffer::CommandBuffer(VkDevice dev,const VkExtent2D &extent,const uint32_t atta_count,VkCommandPool cp,VkCommandBuffer cb)
{
    device=dev;
    pool=cp;
    cmd_buf=cb;

    cv_count=atta_count;

    if(cv_count>0)
    {
        clear_values=hgl_zero_new<VkClearValue>(cv_count);

        clear_values[cv_count-1].depthStencil.depth = 1.0f;
        clear_values[cv_count-1].depthStencil.stencil = 0;
    }
    else
    {
        clear_values=nullptr;
    }

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=extent;

    pipeline_layout=VK_NULL_HANDLE;
}

CommandBuffer::~CommandBuffer()
{
    delete[] clear_values;

    vkFreeCommandBuffers(device,pool,1,&cmd_buf);
}

bool CommandBuffer::Begin()
{
    VkCommandBufferBeginInfo cmd_buf_info;

    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = nullptr;
    cmd_buf_info.flags = 0;
    cmd_buf_info.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmd_buf, &cmd_buf_info)!=VK_SUCCESS)
        return(false);

    return(true);
}

bool CommandBuffer::BeginRenderPass(RenderPass *rp,Framebuffer *fb)
{
    VkRenderPassBeginInfo rp_begin;

    rp_begin.sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext              = nullptr;
    rp_begin.renderPass         = *rp;
    rp_begin.framebuffer        = fb->GetFramebuffer();
    rp_begin.renderArea         = render_area;
    rp_begin.clearValueCount    = cv_count;
    rp_begin.pClearValues       = clear_values;

    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    viewport.x          = 0;
    viewport.y          = 0;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;
    viewport.width      = render_area.extent.width;
    viewport.height     = render_area.extent.height;

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);

    pipeline_layout=VK_NULL_HANDLE;

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
        vkCmdBindIndexBuffer(cmd_buf,indices_buffer->GetBuffer(),render_obj->GetIndexOffset(),indices_buffer->GetType());

    return(true);
}
VK_NAMESPACE_END
