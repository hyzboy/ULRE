#ifndef HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKFence.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
VK_NAMESPACE_BEGIN
class SubmitQueue
{
protected:

    Device *device;
    VkQueue queue;
    
    uint32_t current_fence;
    ObjectList<Fence> fence_list;

    VkSubmitInfo submit_info;

public:

    SubmitQueue(Device *dev,VkQueue q,const uint32_t fence_count=1);
    virtual ~SubmitQueue();
    
    bool Wait(const bool wait_wall=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);
    bool Submit(const VkCommandBuffer &cmd_buf,vulkan::Semaphore *wait_sem,vulkan::Semaphore *complete_sem);
    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,vulkan::Semaphore *wait_sem,vulkan::Semaphore *complete_sem);
};//class SumbitQueue

/**
 * 渲染目标
 */
class RenderTarget:public SubmitQueue
{
protected:

    Framebuffer *fb;
    
    VkExtent2D extent;

protected:

    friend class Device;

    RenderTarget(Device *dev,Framebuffer *_fb,const uint32_t fence_count=1);

public:

    virtual ~RenderTarget()=default;
    
            const VkExtent2D &  GetExtent()const{return extent;}
    virtual const VkRenderPass  GetRenderPass()const{return fb->GetRenderPass();}
    virtual const uint32_t      GetColorCount()const{return fb->GetColorCount();}
    virtual const VkFramebuffer GetFramebuffer()const{return fb->GetFramebuffer();}
};//class RenderTarget

/**
 * 交换链专用渲染目标
 */
class SwapchainRenderTarget:public RenderTarget
{
    Swapchain *swapchain;
    VkSwapchainKHR vk_swapchain;
    VkPresentInfoKHR present_info;

    RenderPass *main_rp=nullptr;

    uint32_t swap_chain_count;

    uint32_t current_frame;
    ObjectList<Framebuffer> render_frame;

public:

    SwapchainRenderTarget(Device *dev,Swapchain *sc);
    ~SwapchainRenderTarget();

    const   VkRenderPass  GetRenderPass()const override{return *main_rp;}
    const   VkFramebuffer GetFramebuffer()const override{return render_frame[current_frame]->GetFramebuffer();}
            VkFramebuffer GetFramebuffer(const uint32_t index){return render_frame[index]->GetFramebuffer();}

    const   uint32_t      GetColorCount()const override{return 1;}
    const   uint32_t      GetImageCount()const{return swap_chain_count;}

    const   uint32_t      GetCurrentFrameIndices()const{return current_frame;}

public:

    /**
     * 请求下一帧画面的索引
     * @param present_complete_semaphore 推送完成信号
     */
    int AcquireNextImage(vulkan::Semaphore *present_complete_semaphore);

    /**
     * 推送后台画面到前台
     * @param render_complete_semaphore 渲染完成信号
     */
    bool PresentBackbuffer(vulkan::Semaphore *render_complete_semaphore);
};//class SwapchainRenderTarget:public RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
