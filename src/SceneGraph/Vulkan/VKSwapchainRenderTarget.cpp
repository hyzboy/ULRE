#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<iostream>
 //#include<iostream>

VK_NAMESPACE_BEGIN
SwapchainRenderTarget::SwapchainRenderTarget(RenderFramework *rf,Swapchain *sc,Semaphore *pcs,RenderTargetData *rtl):MultiFrameRenderTarget(rf,sc->image_count,rtl)
{
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
    
bool SwapchainRenderTarget::NextFrame()
{
//    std::cerr << "[SwapchainRenderTarget] NextFrame current=" << current_frame << " semaphore=" << present_complete_semaphore << std::endl;
    return(vkAcquireNextImageKHR(GetVkDevice(),
                                 swapchain->swap_chain,
                                 UINT64_MAX,
                                 *present_complete_semaphore,
                                 VK_NULL_HANDLE,
                                 &current_frame)==VK_SUCCESS);
}

bool SwapchainRenderTarget::Submit()
{
//    std::cerr << "[SwapchainRenderTarget] Submit frame=" << current_frame << " rtd=" << (rtd_list+current_frame) << std::endl;
    RenderTargetData *rtd=rtd_list+current_frame;

    if(!rtd->Submit(present_complete_semaphore))
        return(false);

    DeviceQueue *queue=GetQueue();

    VkSemaphore wait_semaphores=*rtd->render_complete_semaphore;

    present_info.waitSemaphoreCount =1;
    present_info.pWaitSemaphores    =&wait_semaphores;
    present_info.pImageIndices      =&current_frame;

    VkResult result=queue->Present(&present_info);
//    std::cerr << "[SwapchainRenderTarget] Present result=" << result << std::endl;
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            return false;
        } 
    }

    return(true);
}
VK_NAMESPACE_END
