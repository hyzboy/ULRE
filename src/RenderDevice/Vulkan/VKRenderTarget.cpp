#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKSwapchain.h>

VK_NAMESPACE_BEGIN
namespace 
{
    const VkPipelineStageFlags pipe_stage_flags=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}//namespace

SubmitQueue::SubmitQueue(Device *dev,VkQueue q,const uint32_t fence_count)
{
    device=dev;
    queue=q;

    for(uint32_t i=0;i<fence_count;i++)
         fence_list.Add(device->CreateFence(true));

    current_fence=0;

    submit_info.sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext                   = nullptr;
    //submit_info.waitSemaphoreCount      = 1;
    //submit_info.pWaitSemaphores         = *present_complete_semaphore;
    submit_info.pWaitDstStageMask       = &pipe_stage_flags;
    //submit_info.signalSemaphoreCount    = 1;
    //submit_info.pSignalSemaphores       = *render_complete_semaphore;
}

SubmitQueue::~SubmitQueue()
{
    fence_list.Clear();
}

bool SubmitQueue::Wait(const bool wait_all,uint64_t time_out)
{
    VkFence fence=*fence_list[current_fence];
    
    VkResult result;

    result=vkWaitForFences(device->GetDevice(),1,&fence,wait_all,time_out);
    result=vkResetFences(device->GetDevice(),1,&fence);

    return(true);
}
    
bool SubmitQueue::Submit(VkCommandBuffer &cmd_buf,vulkan::Semaphore *wait_sem,vulkan::Semaphore *complete_sem)
{
    VkSemaphore ws=*wait_sem;
    VkSemaphore cs=*complete_sem;

    submit_info.waitSemaphoreCount  =1;
    submit_info.pWaitSemaphores     =&ws;
    submit_info.commandBufferCount  =1;
    submit_info.pCommandBuffers     =&cmd_buf;
    submit_info.signalSemaphoreCount=1;
    submit_info.pSignalSemaphores   =&cs;
    
    VkFence fence=*fence_list[current_fence];

    VkResult result=vkQueueSubmit(queue,1,&submit_info,fence);

    if(++current_fence==fence_list.GetCount())
        current_fence=0;

    //不在这里立即等待fence完成，是因为有可能queue submit需要久一点工作时间，我们这个时间可以去干别的。等在AcquireNextImage时再去等待fence，而且是另一帧的fence。这样有利于异步处理

    return(result==VK_SUCCESS);
}

RenderTarget::RenderTarget(Device *dev,Framebuffer *_fb,const uint32_t fence_count):SubmitQueue(dev,dev->GetGraphicsQueue(),fence_count)
{
    fb=_fb;
}

SwapchainRenderTarget::SwapchainRenderTarget(Device *dev,Swapchain *sc):RenderTarget(dev,nullptr,sc->GetImageCount())
{
    swapchain=sc;
    vk_swapchain=swapchain->GetSwapchain();

    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext              = nullptr;
    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &vk_swapchain;

    Texture2D **sc_color=swapchain->GetColorTextures();
    Texture2D *sc_depth=swapchain->GetDepthTexture();

    main_rp=device->CreateRenderPass((*sc_color)->GetFormat(),sc_depth->GetFormat());

    swap_chain_count=swapchain->GetImageCount();
    
    extent=swapchain->GetExtent();

    for(uint i=0;i<swap_chain_count;i++)
    {
        render_frame.Add(vulkan::CreateFramebuffer(device,main_rp,(*sc_color)->GetImageView(),sc_depth->GetImageView()));
        ++sc_color;
    }

    current_frame=0;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    render_frame.Clear();

    delete main_rp;
}
    
int SwapchainRenderTarget::AcquireNextImage(vulkan::Semaphore *present_complete_semaphore)
{
    VkSemaphore sem=*present_complete_semaphore;

    if(vkAcquireNextImageKHR(device->GetDevice(),vk_swapchain,UINT64_MAX,sem,VK_NULL_HANDLE,&current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool SwapchainRenderTarget::PresentBackbuffer(vulkan::Semaphore *render_complete_semaphore)
{
    VkSemaphore sem=*render_complete_semaphore;

    present_info.waitSemaphoreCount=1;
    present_info.pWaitSemaphores=&sem;
    present_info.pImageIndices=&current_frame;

    VkResult result=vkQueuePresentKHR(queue,&present_info);
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			
			return false;
		} 
	}

    result=vkQueueWaitIdle(queue);
    
    if(result!=VK_SUCCESS)
        return(false);

    return(true);
}
VK_NAMESPACE_END
