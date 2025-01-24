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

    DeviceQueue *queue;

    RenderPass *render_pass;
    Framebuffer *fbo;
    
    VkExtent2D extent;
    
    Semaphore *render_complete_semaphore =nullptr;

protected:

    uint32_t color_count;               ///<颜色成分数量
    Texture2D **color_textures;         ///<颜色成分纹理列表
    Texture2D *depth_texture;           ///<深度成分纹理

protected:

    friend class GPUDevice;

    RenderTarget(DeviceQueue *,Semaphore *);
    RenderTarget(DeviceQueue *,Semaphore *,RenderPass *_rp,Framebuffer *_fb,Texture2D **color_texture_list,const uint32_t color_count,Texture2D *depth_texture);

public:

    virtual ~RenderTarget();
    
                    DeviceQueue *   GetQueue            ()      {return queue;}
            const   VkExtent2D &    GetExtent           ()const {return extent;}
    virtual         RenderPass *    GetRenderPass       ()      {return render_pass;}
    virtual const   VkRenderPass    GetVkRenderPass     ()const {return render_pass->GetVkRenderPass();}

    virtual const   uint32_t        GetColorCount       ()const {return fbo->GetColorCount();}

    virtual         Framebuffer *   GetFramebuffer      ()      {return fbo;}
    virtual         Texture2D *     GetColorTexture     (const int index=0){return color_textures[index];}
    virtual         Texture2D *     GetDepthTexture     (){return depth_texture;}

public: // command buffer

            Semaphore *     GetRenderCompleteSemaphore  (){return render_complete_semaphore;}
    virtual bool            Submit                      (RenderCmdBuffer *,Semaphore *present_complete_semaphore=nullptr);

            bool            WaitQueue(){return queue->WaitQueue();}
            bool            WaitFence(){return queue->WaitFence();}
};//class RenderTarget

/**
 * 交换链专用渲染目标
 */
class RTSwapchain:public RenderTarget
{
    VkDevice device;
    Swapchain *swapchain;
    PresentInfo present_info;

    Semaphore *present_complete_semaphore=nullptr;

    uint32_t current_frame;

public:

    RTSwapchain(VkDevice dev,Swapchain *sc,DeviceQueue *q,Semaphore *rcs,Semaphore *pcs,RenderPass *rp);
    ~RTSwapchain();

            const   uint32_t        GetColorCount   ()const override            {return 1;}                         ///Swapchain的FBO颜色成份只有一个
            const   uint32_t        GetImageCount   ()const                     {return swapchain->image_count;}

                    Framebuffer *   GetFramebuffer  ()override                  {return swapchain->sc_image[current_frame].fbo;}
                    Framebuffer *   GetFramebuffer  (const int index)           {return swapchain->sc_image[index].fbo;}

    virtual         Texture2D *     GetColorTexture (const int index=0) override{return swapchain->sc_image[current_frame].color;}
    virtual         Texture2D *     GetDepthTexture ()                  override{return swapchain->sc_image[current_frame].depth;}

    RenderCmdBuffer *GetRenderCmdBuffer(const int index)
    {
        return swapchain->sc_image[index].cmd_buf;
    }

public:

            const   uint32_t        GetCurrentFrameIndices      ()const {return current_frame;}
                    Semaphore *     GetPresentCompleteSemaphore ()      {return present_complete_semaphore;}

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
};//class RTSwapchain:public RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
