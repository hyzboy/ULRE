#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
//#include<hgl/graph/vulkan/VKDescriptorSet.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKShaderModuleManage.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
bool ResizeRenderDevice(DeviceAttribute *attr,uint width,uint height);

Device::Device(DeviceAttribute *da)
{
    attr=da;

    current_frame=0;

    texture_fence=this->CreateFence();

    hgl_zero(texture_submitInfo);
    texture_submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    main_rp=nullptr;
    texture_cmd_buf=nullptr;
    RecreateDevice();
}

Device::~Device()
{
    render_frame.Clear();

    delete main_rp;

    delete texture_cmd_buf;
    delete texture_fence;

    delete attr;
}

void Device::RecreateDevice()
{
    render_frame.Clear();
    if(main_rp)delete main_rp;
    if(texture_cmd_buf)delete texture_cmd_buf;

    present.sType           = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext           = nullptr;
    present.swapchainCount  = 1;
    present.pSwapchains     = &attr->swap_chain;
    present.pResults        = nullptr;

    main_rp=CreateRenderPass(attr->sc_image_views[0]->GetFormat(),attr->depth.view->GetFormat());

    const int sc_count=attr->sc_image_views.GetCount();

    for(int i=0;i<sc_count;i++)
    {
        RenderFrame *rf=new RenderFrame;

        rf->frame_buffer                =vulkan::CreateFramebuffer(this,main_rp,attr->sc_image_views[i],attr->depth.view);
        rf->present_complete_semaphore  =this->CreateSem();
        rf->render_complete_semaphore   =this->CreateSem();
        rf->draw_fence                  =this->CreateFence();

        render_frame.Add(rf);
    }

    texture_cmd_buf=CreateCommandBuffer();
}

bool Device::Resize(uint width,uint height)
{
    if(!ResizeRenderDevice(attr,width,height))
        return(false);

    RecreateDevice();
    return(true);
}

CommandBuffer *Device::CreateCommandBuffer()
{
    if(!attr->cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd={};
    cmd.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext=nullptr;
    cmd.commandPool=attr->cmd_pool;
    cmd.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount=1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(attr->device,attr->swapchain_extent,attr->cmd_pool,cmd_buf));
}

RenderPass *Device::CreateRenderPass(VkFormat color_format,VkFormat depth_format)
{
    VkAttachmentDescription attachments[2];

    VkAttachmentReference color_reference={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_reference={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass={};
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags=0;
    subpass.inputAttachmentCount=0;
    subpass.pInputAttachments=nullptr;
    subpass.pResolveAttachments=nullptr;
    subpass.preserveAttachmentCount=0;
    subpass.pPreserveAttachments=nullptr;

    int att_count=0;

    if(color_format!=VK_FORMAT_UNDEFINED)
    {
        attachments[0].format=color_format;
        attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[0].flags=0;

        ++att_count;

        subpass.colorAttachmentCount=1;
        subpass.pColorAttachments=&color_reference;
    }
    else
    {
        subpass.colorAttachmentCount=0;
        subpass.pColorAttachments=nullptr;
    }

    if(depth_format!=VK_FORMAT_UNDEFINED)
    {
        attachments[att_count].format=depth_format;
        attachments[att_count].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[att_count].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[att_count].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[att_count].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[att_count].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[att_count].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[att_count].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[att_count].flags=0;

        depth_reference.attachment=att_count;

        ++att_count;

        subpass.pDepthStencilAttachment=&depth_reference;
    }
    else
    {
        subpass.pDepthStencilAttachment=nullptr;
    }

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_info={};
    rp_info.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext=nullptr;
    rp_info.attachmentCount=att_count;
    rp_info.pAttachments=attachments;
    rp_info.subpassCount=1;
    rp_info.pSubpasses=&subpass;
    rp_info.dependencyCount=1;
    rp_info.pDependencies=&dependency;

    VkRenderPass render_pass;

    if(vkCreateRenderPass(attr->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(attr->device,render_pass,color_format,depth_format));
}

Fence *Device::CreateFence()
{
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new Fence(attr->device,fence));
}

Semaphore *Device::CreateSem()
{
    VkSemaphoreCreateInfo SemaphoreCreateInfo;
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    SemaphoreCreateInfo.pNext = nullptr;
    SemaphoreCreateInfo.flags = 0;

    VkSemaphore sem;
    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new Semaphore(attr->device,sem));
}

ShaderModuleManage *Device::CreateShaderModuleManage()
{
    return(new ShaderModuleManage(this));
}

bool Device::AcquireNextImage()
{
    VkSemaphore present_complete_semaphore=*(render_frame[current_frame]->present_complete_semaphore);

    return(vkAcquireNextImageKHR(attr->device,attr->swap_chain,UINT64_MAX,present_complete_semaphore,VK_NULL_HANDLE,&current_frame)==VK_SUCCESS);
}

bool Device::SubmitDraw(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};

    RenderFrame *rf=render_frame[current_frame];

    VkSemaphore present_sem=*(rf->present_complete_semaphore);
    VkSemaphore complete_sem=*(rf->render_complete_semaphore);

    submit_info.pNext = nullptr;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &present_sem;
    submit_info.pWaitDstStageMask = &pipe_stage_flags;
    submit_info.commandBufferCount = count;
    submit_info.pCommandBuffers = cmd_bufs;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &complete_sem;

    VkFence fence=*(rf->draw_fence);

    return(vkQueueSubmit(attr->graphics_queue,1,&submit_info,fence)==VK_SUCCESS);
}

bool Device::Wait(bool wait_all,uint64_t time_out)
{
    VkFence fence=*(render_frame[current_frame]->draw_fence);
    
    while(vkWaitForFences(attr->device,1,&fence,wait_all,time_out)==VK_TIMEOUT){}
    vkResetFences(attr->device,1,&fence);

    return(true);
}

bool Device::QueuePresent()
{
    VkSemaphore render_complete_semaphore=*(render_frame[current_frame]->render_complete_semaphore);

    present.pImageIndices = &current_frame;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_complete_semaphore;

    return(vkQueuePresentKHR(attr->present_queue, &present)==VK_SUCCESS);
}
VK_NAMESPACE_END
