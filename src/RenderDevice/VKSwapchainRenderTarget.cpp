#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>

VK_NAMESPACE_BEGIN
SwapchainRenderTarget::SwapchainRenderTarget(GPUDevice *dev,Swapchain *sc):RenderTarget(dev,nullptr,sc->GetImageCount())
{
    swapchain=sc;
    vk_swapchain=swapchain->GetSwapchain();

    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &vk_swapchain;

    Texture2D **sc_color=swapchain->GetColorTextures();
    Texture2D *sc_depth=swapchain->GetDepthTexture();

    SwapchainRenderbufferInfo rbi((*sc_color)->GetFormat(),sc_depth->GetFormat());

    this->render_pass=device->CreateRenderPass(&rbi);

    swap_chain_count=swapchain->GetImageCount();
    
    extent=swapchain->GetExtent();

    render_frame=new Framebuffer *[swap_chain_count];

    for(uint i=0;i<swap_chain_count;i++)
    {
        render_frame[i]=device->CreateFramebuffer(this->render_pass,(*sc_color)->GetImageView(),sc_depth->GetImageView());
        ++sc_color;
    }

    current_frame=0;

    present_complete_semaphore=dev->CreateSemaphore();
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    SAFE_CLEAR_OBJECT_ARRAY(render_frame,swap_chain_count);

    delete present_complete_semaphore;
}
    
int SwapchainRenderTarget::AcquireNextImage()
{
    if(vkAcquireNextImageKHR(device->GetDevice(),vk_swapchain,UINT64_MAX,*(this->present_complete_semaphore),VK_NULL_HANDLE,&current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool SwapchainRenderTarget::PresentBackbuffer(VkSemaphore *wait_semaphores,const uint32_t count)
{
    present_info.waitSemaphoreCount =count;
    present_info.pWaitSemaphores    =wait_semaphores;
    present_info.pImageIndices      =&current_frame;

    VkResult result=vkQueuePresentKHR(queue,&present_info);
    
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			
			return false;
		} 
	}

    return(true);
}

bool SwapchainRenderTarget::PresentBackbuffer()
{
    VkSemaphore sem=*render_complete_semaphore;

    return this->PresentBackbuffer(&sem,1);
}
    
bool SwapchainRenderTarget::Submit(VkCommandBuffer cb)
{
    return GPUQueue::Submit(cb,present_complete_semaphore,render_complete_semaphore);
}

bool SwapchainRenderTarget::Submit(VkCommandBuffer cb,GPUSemaphore *pce)
{
    return GPUQueue::Submit(cb,pce,render_complete_semaphore);
}
VK_NAMESPACE_END
