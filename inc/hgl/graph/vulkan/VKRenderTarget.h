#ifndef HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKSubmitQueue.h>
VK_NAMESPACE_BEGIN
/**
 * 渲染目标
 */
class RenderTarget:public SubmitQueue
{
protected:

    Framebuffer *fb;
    
    VkExtent2D extent;
    
    GPUSemaphore *  render_complete_semaphore   =nullptr;
    CommandBuffer * command_buffer              =nullptr;

protected:

    ObjectList<Texture2D> color_texture;
    Texture2D *depth_texture;

protected:

    friend class Device;

    RenderTarget(Device *dev,Framebuffer *_fb,CommandBuffer *_cb,const uint32_t fence_count=1);
    RenderTarget(Device *dev,Framebuffer *_fb,CommandBuffer *_cb,Texture2D **color_texture_list,const uint32_t color_count,Texture2D *depth_texture,const uint32_t fence_count=1);

public:

    virtual ~RenderTarget();
    
            const   VkExtent2D &    GetExtent           ()const {return extent;}
                    GPUSemaphore *  GetCompleteSemaphore(){return render_complete_semaphore;}
                    CommandBuffer * GetCommandBuffer    ()      {return command_buffer;}
    virtual const   VkRenderPass    GetRenderPass       ()const {return fb->GetRenderPass();}
    virtual const   uint32_t        GetColorCount       ()const {return fb->GetColorCount();}
    virtual const   VkFramebuffer   GetFramebuffer      ()const {return fb->GetFramebuffer();}

    virtual         Texture2D *     GetColorTexture     (const int index=0){return color_texture[index];}
    virtual         Texture2D *     GetDepthTexture     (){return depth_texture;}

    virtual         bool            Submit              (GPUSemaphore *present_complete_semaphore=nullptr);
};//class RenderTarget

/**
 * 交换链专用渲染目标
 */
class SwapchainRenderTarget:public RenderTarget
{
    Swapchain *swapchain;
    VkSwapchainKHR vk_swapchain;
    PresentInfo present_info;

    GPUSemaphore *present_complete_semaphore=nullptr;

    RenderPass *main_rp=nullptr;

    uint32_t swap_chain_count;

    uint32_t current_frame;
    ObjectList<Framebuffer> render_frame;

public:

    SwapchainRenderTarget(Device *dev,Swapchain *sc);
    ~SwapchainRenderTarget();

            const   VkRenderPass    GetRenderPass   ()const override        {return *main_rp;}
            const   VkFramebuffer   GetFramebuffer  ()const override        {return render_frame[current_frame]->GetFramebuffer();}
                    VkFramebuffer   GetFramebuffer  (const uint32_t index)  {return render_frame[index]->GetFramebuffer();}

            const   uint32_t        GetColorCount   ()const override   {return 1;}
            const   uint32_t        GetImageCount   ()const            {return swap_chain_count;}

            const   uint32_t        GetCurrentFrameIndices()const{return current_frame;}
                    GPUSemaphore *  GetPresentCompleteSemaphore(){return present_complete_semaphore;}

    virtual         Texture2D *     GetColorTexture(const int index=0)  override{return swapchain->GetColorTexture(index);}
    virtual         Texture2D *     GetDepthTexture()                   override{return swapchain->GetDepthTexture();}

public:

    /**
     * 请求下一帧画面的索引
     * @param present_complete_semaphore 推送完成信号
     */
    int AcquireNextImage();

    /**
     * 推送后台画面到前台
     * @param render_complete_semaphore 渲染完成信号
     */
    bool PresentBackbuffer(VkSemaphore *wait_semaphores,const uint32_t wait_semaphore_count);

    bool PresentBackbuffer();

    bool Submit(VkCommandBuffer);
};//class SwapchainRenderTarget:public RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
