#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>

VK_NAMESPACE_BEGIN
SwapchainRenderTarget::SwapchainRenderTarget(VkDevice dev,Swapchain *sc,Semaphore *pcs,RenderTarget **rtl):MFRenderTarget(sc->image_count,rtl)
{
    device=dev;

    swapchain=sc;

    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &(swapchain->swap_chain);
   
    present_complete_semaphore=pcs;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    delete present_complete_semaphore;
    delete swapchain;
}
    
int SwapchainRenderTarget::AcquireNextImage()
{
    if(vkAcquireNextImageKHR(device,
                             swapchain->swap_chain,
                             UINT64_MAX,
                             *present_complete_semaphore,
                             VK_NULL_HANDLE,
                             &current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool SwapchainRenderTarget::PresentBackbuffer()
{
    DeviceQueue *queue=GetQueue();

    VkSemaphore wait_semaphores=*GetRenderCompleteSemaphore();

    present_info.waitSemaphoreCount =1;
    present_info.pWaitSemaphores    =&wait_semaphores;
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
VK_NAMESPACE_END
