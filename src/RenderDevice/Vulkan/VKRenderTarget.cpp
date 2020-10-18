#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/vulkan/VKSemaphore.h>

VK_NAMESPACE_BEGIN
RenderTarget::RenderTarget(Device *dev,Framebuffer *_fb,CommandBuffer *_cb,const uint32_t fence_count):SubmitQueue(dev,dev->GetGraphicsQueue(),fence_count)
{
    rp=nullptr;
    fb=_fb;
    command_buffer=_cb;

    if(fb)
        color_count=fb->GetColorCount();
    else
        color_count=0;

    color_textures=nullptr;
    depth_texture=nullptr;    
    render_complete_semaphore=dev->CreateSemaphore();
}

RenderTarget::RenderTarget(Device *dev,RenderPass *_rp,Framebuffer *_fb,CommandBuffer *_cb,Texture2D **ctl,const uint32_t cc,Texture2D *dt,const uint32_t fence_count):SubmitQueue(dev,dev->GetGraphicsQueue(),fence_count)
{
    rp=_rp;
    fb=_fb;
    command_buffer=_cb;
    
    depth_texture=dt;

    color_count=cc;
    if(color_count>0)
    {
        color_textures=new Texture2D *[color_count];
        hgl_cpy<Texture2D *>(color_textures,ctl,color_count);

        extent.width=color_textures[0]->GetWidth();
        extent.height=color_textures[0]->GetHeight();
    }
    else
    {
        color_textures=nullptr;

        if(depth_texture)
        {
            extent.width=depth_texture->GetWidth();
            extent.height=depth_texture->GetHeight();
        }
    }

    render_complete_semaphore=dev->CreateSemaphore();
}

RenderTarget::~RenderTarget()
{    
    SAFE_CLEAR(depth_texture);
    SAFE_CLEAR_OBJECT_ARRAY(color_textures,color_count);
    
    SAFE_CLEAR(render_complete_semaphore);
    SAFE_CLEAR(command_buffer);
    SAFE_CLEAR(fb);
    SAFE_CLEAR(rp);
}

bool RenderTarget::Submit(GPUSemaphore *present_complete_semaphore)
{
    return this->SubmitQueue::Submit(*command_buffer,present_complete_semaphore,render_complete_semaphore);
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

    main_rp=device->CreateRenderPass((*sc_color)->GetFormat(),sc_depth->GetFormat(),VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    swap_chain_count=swapchain->GetImageCount();
    
    extent=swapchain->GetExtent();

    render_frame=new Framebuffer *[swap_chain_count];

    for(uint i=0;i<swap_chain_count;i++)
    {
        render_frame[i]=device->CreateFramebuffer(main_rp,(*sc_color)->GetImageView(),sc_depth->GetImageView());
        ++sc_color;
    }

    current_frame=0;

    present_complete_semaphore=dev->CreateSemaphore();
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    SAFE_CLEAR_OBJECT_ARRAY(render_frame,swap_chain_count);

    delete present_complete_semaphore;
    delete main_rp;
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
    return SubmitQueue::Submit(cb,present_complete_semaphore,render_complete_semaphore);
}

bool SwapchainRenderTarget::Submit(VkCommandBuffer cb,GPUSemaphore *pce)
{
    return SubmitQueue::Submit(cb,pce,render_complete_semaphore);
}
VK_NAMESPACE_END