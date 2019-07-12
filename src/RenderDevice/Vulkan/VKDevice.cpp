#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/vulkan/VKTexture.h>
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

    texture_fence=this->CreateFence(false);

    hgl_zero(texture_submit_info);
    texture_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    main_rp=nullptr;
    texture_cmd_buf=nullptr;
    
    pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    present_complete_semaphore  =this->CreateSem();
    render_complete_semaphore   =this->CreateSem();
    
    submit_info.sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext                   = nullptr;
    submit_info.waitSemaphoreCount      = 1;
    submit_info.pWaitSemaphores         = *present_complete_semaphore;
    submit_info.pWaitDstStageMask       = &pipe_stage_flags;
    submit_info.signalSemaphoreCount    = 1;
    submit_info.pSignalSemaphores       = *render_complete_semaphore;
    
    present_info.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext               = nullptr;
    present_info.waitSemaphoreCount  = 1;
    present_info.pWaitSemaphores     = *render_complete_semaphore;
    present_info.swapchainCount      = 1;
    present_info.pResults            = nullptr;

    RecreateDevice();
}

Device::~Device()
{
    fence_list.Clear();
    render_frame.Clear();

    delete present_complete_semaphore;
    delete render_complete_semaphore;

    delete main_rp;

    delete texture_cmd_buf;
    delete texture_fence;

    delete attr;
}

void Device::RecreateDevice()
{
    fence_list.Clear();
    render_frame.Clear();

    if(main_rp)delete main_rp;
    if(texture_cmd_buf)delete texture_cmd_buf;

    present_info.pSwapchains=&attr->swap_chain;

    main_rp=CreateRenderPass(attr->format,attr->sc_depth->GetFormat());

    swap_chain_count=attr->sc_texture.GetCount();

    for(uint i=0;i<swap_chain_count;i++)
    {
        render_frame.Add(vulkan::CreateFramebuffer(this,main_rp,attr->sc_texture[i]->GetImageView(),attr->sc_depth->GetImageView()));
        fence_list.Add(this->CreateFence(true));
    }

    texture_cmd_buf=CreateCommandBuffer(nullptr,0);
    
    current_frame=0;
    current_fence=0;
}

bool Device::Resize(uint width,uint height)
{
    if(!ResizeRenderDevice(attr,width,height))
        return(false);

    RecreateDevice();
    return(true);
}

CommandBuffer *Device::CreateCommandBuffer(const VkExtent2D *extent,const uint32_t atta_count)
{
    if(!attr->cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd;
    cmd.sType               =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext               =nullptr;
    cmd.commandPool         =attr->cmd_pool;
    cmd.level               =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount  =1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(attr->device,extent?*extent:attr->swapchain_extent,atta_count,attr->cmd_pool,cmd_buf));
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
Fence *Device::CreateFence(bool create_signaled)
{
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0;

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

bool Device::Wait(bool wait_all,uint64_t time_out)
{
    VkFence fence=*fence_list[current_fence];
    
    VkResult result;

    result=vkWaitForFences(attr->device,1,&fence,wait_all,time_out);
    result=vkResetFences(attr->device,1,&fence);

    return(true);
}

bool Device::AcquireNextImage(VkSemaphore present_complete_semaphore)
{
    return(vkAcquireNextImageKHR(attr->device,attr->swap_chain,UINT64_MAX,present_complete_semaphore,VK_NULL_HANDLE,&current_frame)==VK_SUCCESS);
}

bool Device::SubmitDraw(List<VkCommandBuffer> &cmd_lists,List<VkSemaphore> &wait_sems,List<VkSemaphore> &complete_sems)
{
    if(cmd_lists.GetCount()<=0)
        return(false);
    
    submit_info.waitSemaphoreCount  =wait_sems.GetCount();
    submit_info.pWaitSemaphores     =wait_sems.GetData();
    submit_info.commandBufferCount  =cmd_lists.GetCount();
    submit_info.pCommandBuffers     =cmd_lists.GetData();
    submit_info.signalSemaphoreCount=complete_sems.GetCount();
    submit_info.pSignalSemaphores   =complete_sems.GetData();

    VkFence fence=*fence_list[current_fence];

    VkResult result=vkQueueSubmit(attr->graphics_queue,1,&submit_info,fence);

    if(++current_fence==swap_chain_count)
        current_fence=0;

    //不在这里立即等待fence完成，是因为有可能queue submit需要久一点工作时间，我们这个时间可以去干别的。等在AcquireNextImage时再去等待fence，而且是另一帧的fence。这样有利于异步处理

    return(result==VK_SUCCESS);
}

bool Device::PresentBackbuffer()
{
    present_info.pImageIndices=&current_frame;

    VkResult result=vkQueuePresentKHR(attr->graphics_queue,&present_info);
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			
			return false;
		} 
	}

    result=vkQueueWaitIdle(attr->graphics_queue);
    
    if(result!=VK_SUCCESS)
        return(false);

    return(true);
}
VK_NAMESPACE_END
