#ifndef HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKQueue.h>
#include<hgl/graph/VKPipeline.h>
VK_NAMESPACE_BEGIN
/**
 * 渲染目标
 */
class RenderTarget
{
protected:

    Queue *queue;

    RenderPass *render_pass;
    Framebuffer *fbo;
    
    VkExtent2D extent;
    
    Semaphore *render_complete_semaphore =nullptr;

protected:

    uint32_t color_count;
    Texture2D **color_textures;
    Texture2D *depth_texture;

protected:

    friend class GPUDevice;

    RenderTarget(Queue *,Semaphore *);
    RenderTarget(Queue *,Semaphore *,RenderPass *_rp,Framebuffer *_fb,Texture2D **color_texture_list,const uint32_t color_count,Texture2D *depth_texture);

public:

    virtual ~RenderTarget();
    
                    Queue *      GetQueue            ()      {return queue;}
            const   VkExtent2D &    GetExtent           ()const {return extent;}
    virtual         RenderPass *    GetRenderPass       ()      {return render_pass;}
    virtual const   VkRenderPass    GetVkRenderPass     ()const {return render_pass->GetVkRenderPass();}
    virtual const   uint32_t        GetColorCount       ()const {return fbo->GetColorCount();}
    virtual         Framebuffer *   GetFramebuffer      ()      {return fbo;}

    virtual         Texture2D *     GetColorTexture     (const int index=0){return color_textures[index];}
    virtual         Texture2D *     GetDepthTexture     (){return depth_texture;}

public: // command buffer

            Semaphore *  GetRenderCompleteSemaphore  (){return render_complete_semaphore;}
    virtual bool            Submit                      (RenderCmdBuffer *,Semaphore *present_complete_semaphore=nullptr);

            bool            WaitQueue(){return queue->WaitQueue();}
            bool            WaitFence(){return queue->WaitFence();}
};//class RenderTarget

/**
 * 交换链专用渲染目标
 */
class SwapchainRenderTarget:public RenderTarget
{
    VkDevice device;
    Swapchain *swapchain;
    PresentInfo present_info;

    Semaphore *present_complete_semaphore=nullptr;

    uint32_t current_frame;

public:

    SwapchainRenderTarget(VkDevice dev,Swapchain *sc,Queue *q,Semaphore *rcs,Semaphore *pcs,RenderPass *rp);
    ~SwapchainRenderTarget();

                    Framebuffer *   GetFramebuffer  ()override                  {return swapchain->render_frame[current_frame];}
                    Framebuffer *   GetFramebuffer  (const uint32_t index)      {return swapchain->render_frame[index];}

            const   uint32_t        GetColorCount   ()const override            {return 1;}
            const   uint32_t        GetImageCount   ()const                     {return swapchain->color_count;}

    virtual         Texture2D *     GetColorTexture (const int index=0) override{return swapchain->sc_color[index];}
    virtual         Texture2D *     GetDepthTexture ()                  override{return swapchain->sc_depth;}

public:

            const   uint32_t        GetCurrentFrameIndices      ()const {return current_frame;}
                    Semaphore *  GetPresentCompleteSemaphore ()      {return present_complete_semaphore;}

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
    bool Submit(VkCommandBuffer,Semaphore *);
};//class SwapchainRenderTarget:public RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
