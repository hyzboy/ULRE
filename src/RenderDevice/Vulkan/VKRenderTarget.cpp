#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN
namespace 
{
    const VkPipelineStageFlags pipe_stage_flags=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}//namespace

RenderTarget::RenderTarget(Device *dev,Framebuffer *_fb,CommandBuffer *_cb,const uint32_t fence_count):SubmitQueue(dev,dev->GetGraphicsQueue(),fence_count)
{
    fb=_fb;
    command_buffer=_cb;

    depth_texture=nullptr;
}

RenderTarget::RenderTarget(Device *dev,Framebuffer *_fb,CommandBuffer *_cb,Texture2D **ctl,const uint32_t cc,Texture2D *dt,const uint32_t fence_count):SubmitQueue(dev,dev->GetGraphicsQueue(),fence_count)
{
    fb=_fb;
    command_buffer=_cb;

    color_texture.Add(ctl,cc);
    depth_texture=dt;
}

RenderTarget::~RenderTarget()
{
    SAFE_CLEAR(depth_texture);
    color_texture.Clear();

    SAFE_CLEAR(command_buffer);
}

SwapchainRenderTarget::SwapchainRenderTarget(Device *dev,Swapchain *sc):RenderTarget(dev,nullptr,nullptr,sc->GetImageCount())
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

    main_rp=device->CreateRenderPass((*sc_color)->GetFormat(),sc_depth->GetFormat());

    swap_chain_count=swapchain->GetImageCount();
    
    extent=swapchain->GetExtent();

    for(uint i=0;i<swap_chain_count;i++)
    {
        render_frame.Add(device->CreateFramebuffer(main_rp,(*sc_color)->GetImageView(),sc_depth->GetImageView()));
        ++sc_color;
    }

    current_frame=0;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    render_frame.Clear();

    delete main_rp;
}
    
int SwapchainRenderTarget::AcquireNextImage(VkSemaphore present_complete_semaphore)
{
    if(vkAcquireNextImageKHR(device->GetDevice(),vk_swapchain,UINT64_MAX,present_complete_semaphore,VK_NULL_HANDLE,&current_frame)==VK_SUCCESS)
        return current_frame;

    return -1;
}

bool SwapchainRenderTarget::PresentBackbuffer(VkSemaphore *render_complete_semaphore,const uint32_t count)
{
    present_info.waitSemaphoreCount =count;
    present_info.pWaitSemaphores    =render_complete_semaphore;
    present_info.pImageIndices      =&current_frame;

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
