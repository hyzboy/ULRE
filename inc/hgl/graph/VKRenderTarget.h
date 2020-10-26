#ifndef HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKQueue.h>
VK_NAMESPACE_BEGIN
/**
 * 渲染目标
 */
class RenderTarget:public GPUQueue
{
protected:

    RenderPass *render_pass;
    Framebuffer *fbo;
    
    VkExtent2D extent;
    
    GPUSemaphore *render_complete_semaphore =nullptr;
    GPUCmdBuffer *command_buffer            =nullptr;

protected:

    uint32_t color_count;
    Texture2D **color_textures;
    Texture2D *depth_texture;

protected:

    friend class GPUDevice;

    RenderTarget(GPUDevice *dev,Framebuffer *_fb,GPUCmdBuffer *_cb,const uint32_t fence_count=1);
    RenderTarget(GPUDevice *dev,RenderPass *_rp,Framebuffer *_fb,GPUCmdBuffer *_cb,Texture2D **color_texture_list,const uint32_t color_count,Texture2D *depth_texture,const uint32_t fence_count=1);

public:

    virtual ~RenderTarget();
    
            const   VkExtent2D &    GetExtent           ()const {return extent;}
    virtual const   VkRenderPass    GetRenderPass       ()const {return *render_pass;}
    virtual const   uint32_t        GetColorCount       ()const {return fbo->GetColorCount();}
    virtual const   VkFramebuffer   GetFramebuffer      ()const {return fbo->GetFramebuffer();}

    virtual         Texture2D *     GetColorTexture     (const int index=0){return color_textures[index];}
    virtual         Texture2D *     GetDepthTexture     (){return depth_texture;}

public:

            GPUSemaphore *  GetRenderCompleteSemaphore  (){return render_complete_semaphore;}
            GPUCmdBuffer *  GetCommandBuffer            (){return command_buffer;}
    virtual bool            Submit                      (GPUSemaphore *present_complete_semaphore=nullptr);
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

    uint32_t swap_chain_count;

    uint32_t current_frame;
    Framebuffer **render_frame;

public:

    SwapchainRenderTarget(GPUDevice *dev,Swapchain *sc);
    ~SwapchainRenderTarget();

            const   VkFramebuffer   GetFramebuffer  ()const override            {return render_frame[current_frame]->GetFramebuffer();}
                    VkFramebuffer   GetFramebuffer  (const uint32_t index)      {return render_frame[index]->GetFramebuffer();}

            const   uint32_t        GetColorCount   ()const override            {return 1;}
            const   uint32_t        GetImageCount   ()const                     {return swap_chain_count;}

    virtual         Texture2D *     GetColorTexture (const int index=0) override{return swapchain->GetColorTexture(index);}
    virtual         Texture2D *     GetDepthTexture ()                  override{return swapchain->GetDepthTexture();}

public:

            const   uint32_t        GetCurrentFrameIndices      ()const {return current_frame;}
                    GPUSemaphore *  GetPresentCompleteSemaphore ()      {return present_complete_semaphore;}

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
    bool Submit(VkCommandBuffer,GPUSemaphore *);
};//class SwapchainRenderTarget:public RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
