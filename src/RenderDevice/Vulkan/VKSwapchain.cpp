﻿#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKRenderPass.h>

VK_NAMESPACE_BEGIN
Swapchain::Swapchain(Device *dev,SwapchainAttribute *sa)
{
    device=dev;
    sc_attr=sa;
    
    pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    current_frame=0;
    main_rp=nullptr;
    
    present_complete_semaphore  =device->CreateSem();
    render_complete_semaphore   =device->CreateSem();
    
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
}

Swapchain::~Swapchain()
{
    fence_list.Clear();
    render_frame.Clear();

    delete present_complete_semaphore;
    delete render_complete_semaphore;

    delete main_rp;

    SAFE_CLEAR(sc_attr);
}

void Swapchain::Recreate()
{
    fence_list.Clear();
    render_frame.Clear();

    if(main_rp)delete main_rp;

    present_info.pSwapchains=&(sc_attr->swap_chain);

    main_rp=device->CreateRenderPass(sc_attr->sc_color[0]->GetFormat(),sc_attr->sc_depth->GetFormat());

    for(uint i=0;i<sc_attr->swap_chain_count;i++)
    {
        render_frame.Add(vulkan::CreateFramebuffer(device,main_rp,sc_attr->sc_color[i]->GetImageView(),sc_attr->sc_depth->GetImageView()));
        fence_list.Add(device->CreateFence(true));
    }
    
    current_frame=0;
    current_fence=0;
}

bool Swapchain::Wait(bool wait_all,uint64_t time_out)
{
    VkFence fence=*fence_list[current_fence];
    
    VkResult result;

    result=vkWaitForFences(device->GetDevice(),1,&fence,wait_all,time_out);
    result=vkResetFences(device->GetDevice(),1,&fence);

    return(true);
}

int Swapchain::AcquireNextImage(VkSemaphore present_complete_semaphore)
{
    if(vkAcquireNextImageKHR(device->GetDevice(),sc_attr->swap_chain,UINT64_MAX,present_complete_semaphore,VK_NULL_HANDLE,&current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool Swapchain::SubmitDraw(VkCommandBuffer &cmd_list,VkSemaphore &wait_sem,VkSemaphore &complete_sem)
{
    submit_info.waitSemaphoreCount  =1;
    submit_info.pWaitSemaphores     =&wait_sem;
    submit_info.commandBufferCount  =1;
    submit_info.pCommandBuffers     =&cmd_list;
    submit_info.signalSemaphoreCount=1;
    submit_info.pSignalSemaphores   =&complete_sem;

    VkFence fence=*fence_list[current_fence];

    VkResult result=vkQueueSubmit(sc_attr->graphics_queue,1,&submit_info,fence);

    if(++current_fence==sc_attr->swap_chain_count)
        current_fence=0;

    //不在这里立即等待fence完成，是因为有可能queue submit需要久一点工作时间，我们这个时间可以去干别的。等在AcquireNextImage时再去等待fence，而且是另一帧的fence。这样有利于异步处理

    return(result==VK_SUCCESS);
}

bool Swapchain::SubmitDraw(List<VkCommandBuffer> &cmd_lists,List<VkSemaphore> &wait_sems,List<VkSemaphore> &complete_sems)
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

    VkResult result=vkQueueSubmit(sc_attr->graphics_queue,1,&submit_info,fence);

    if(++current_fence==sc_attr->swap_chain_count)
        current_fence=0;

    //不在这里立即等待fence完成，是因为有可能queue submit需要久一点工作时间，我们这个时间可以去干别的。等在AcquireNextImage时再去等待fence，而且是另一帧的fence。这样有利于异步处理

    return(result==VK_SUCCESS);
}

bool Swapchain::PresentBackbuffer()
{
    present_info.pImageIndices=&current_frame;

    VkResult result=vkQueuePresentKHR(sc_attr->graphics_queue,&present_info);
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			
			return false;
		} 
	}

    result=vkQueueWaitIdle(sc_attr->graphics_queue);
    
    if(result!=VK_SUCCESS)
        return(false);

    return(true);
}
VK_NAMESPACE_END
