#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
//#include<iostream>

VK_NAMESPACE_BEGIN
SwapchainRenderTarget::SwapchainRenderTarget(RenderFramework *rf,Swapchain *sc,Semaphore *pcs,RenderTarget **rtl):MultiFrameRenderTarget(rf,sc->image_count,rtl)
{
    swapchain=sc;

    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &(swapchain->swap_chain);
   
    present_complete_semaphore=pcs;

    VkSemaphore sem=*present_complete_semaphore;

//    std::cout<<"present complete semaphore : "<<std::hex<<sem<<std::endl;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    delete present_complete_semaphore;
    delete swapchain;
}
    
IRenderTarget *SwapchainRenderTarget::AcquireNextImage()
{
    VkSemaphore sem=*present_complete_semaphore;

    //std::cout<<"AcquireNextImage present_complete_semaphore : "<<std::hex<<sem<<std::endl;

    if(vkAcquireNextImageKHR(GetVkDevice(),
                             swapchain->swap_chain,
                             UINT64_MAX,
                             sem,
                             VK_NULL_HANDLE,
                             &current_frame)!=VK_SUCCESS)
        return(nullptr);

    //std::cerr<<"AcquireNextImage current_frame="<<current_frame<<std::endl;

    return rt_list[current_frame];
}

bool SwapchainRenderTarget::Submit()
{
    IRenderTarget *rt=rt_list[current_frame];
    
    //std::cout<<"submit frame="<<current_frame<<std::endl;

    if(!rt->Submit(present_complete_semaphore))
        return(false);

    DeviceQueue *queue=GetQueue();

    VkSemaphore wait_semaphores=*rt->GetRenderCompleteSemaphore();

    present_info.waitSemaphoreCount =1;
    present_info.pWaitSemaphores    =&wait_semaphores;
    present_info.pImageIndices      =&current_frame;

//    std::cout<<"present frame="<<current_frame<<std::endl;

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
