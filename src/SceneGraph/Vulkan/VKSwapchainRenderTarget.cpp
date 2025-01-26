#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>

VK_NAMESPACE_BEGIN
RTSwapchain::RTSwapchain(VkDevice dev,Swapchain *sc,DeviceQueue *q,Semaphore *rcs,Semaphore *pcs):RenderTarget(q,rcs)
{
    device=dev;

    swapchain=sc;

    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &(swapchain->swap_chain);

    extent=swapchain->extent;

    current_frame=0;

    present_complete_semaphore=pcs;
}

RTSwapchain::~RTSwapchain()
{
    delete present_complete_semaphore;
    delete swapchain;
}
    
int RTSwapchain::AcquireNextImage()
{
    if(vkAcquireNextImageKHR(device,swapchain->swap_chain,UINT64_MAX,*(this->present_complete_semaphore),VK_NULL_HANDLE,&current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool RTSwapchain::PresentBackbuffer(VkSemaphore *wait_semaphores,const uint32_t count)
{
    present_info.waitSemaphoreCount =count;
    present_info.pWaitSemaphores    =wait_semaphores;
    present_info.pImageIndices      =&current_frame;

    VkResult result=queue->Present(&present_info);
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			
			return false;
		} 
	}

    return(true);
}

bool RTSwapchain::PresentBackbuffer()
{
    VkSemaphore sem=*render_complete_semaphore;

    return this->PresentBackbuffer(&sem,1);
}
    
bool RTSwapchain::Submit(VkCommandBuffer cb)
{
    return queue->Submit(cb,present_complete_semaphore,render_complete_semaphore);
}

bool RTSwapchain::Submit(VkCommandBuffer cb,Semaphore *pce)
{
    return queue->Submit(cb,pce,render_complete_semaphore);
}
VK_NAMESPACE_END
