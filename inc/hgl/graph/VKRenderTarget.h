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
* RenderTarget 存在几种情况：
* 
* 1.正常单帧渲染目标，即只有一帧的数据，每次渲染都是当前帧
* 
* 2.多帧渲染目标，即有多帧数据，每次渲染都是指定帧，典型的是Swapchain
* 
* 所以RenderTarget的其实是一个多态类，根据不同的情况，有不同的实现
*/

class IRenderTarget
{
public:

    IRenderTarget(DeviceQueue *,Semaphore *);

};//class IRenderTarget

/**
 * 渲染目标
 */
class RenderTarget
{
protected:

    DeviceQueue *queue;

    Framebuffer *fbo;
    
    VkExtent2D extent;
    
    Semaphore *render_complete_semaphore =nullptr;

protected:

    uint32_t color_count;

    Texture2D **color_textures;
    Texture2D *depth_texture;

protected:

    friend class RenderTargetManager;

    RenderTarget(DeviceQueue *,Semaphore *);
    RenderTarget(DeviceQueue *,Semaphore *,Framebuffer *_fb,Texture2D **color_texture_list,const uint32_t color_count,Texture2D *depth_texture);

public:

    virtual ~RenderTarget();
    
                    DeviceQueue *   GetQueue            ()      {return queue;}
            const   VkExtent2D &    GetExtent           ()const {return extent;}
    virtual         RenderPass *    GetRenderPass       ()      {return GetFramebuffer()->GetRenderPass();}
    virtual         uint32_t        GetColorCount       ()      {return GetFramebuffer()->GetColorCount();}

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
    
            uint32_t            GetColorCount           ()      override            {return 1;}
            uint32_t            GetImageCount           ()const                     {return swapchain->image_count;}
            uint32_t            GetCurrentFrameIndices  ()const                     {return current_frame;}

            Framebuffer *       GetFramebuffer          ()override                  {return swapchain->sc_image[current_frame].fbo;}
            Framebuffer *       GetFramebuffer          (int index)                 {return swapchain->sc_image[index].fbo;}
           
    virtual Texture2D *         GetColorTexture         (const int index=0) override{return swapchain->sc_image[current_frame].color;}
    virtual Texture2D *         GetDepthTexture         ()                  override{return swapchain->sc_image[current_frame].depth;}

            RenderCmdBuffer *   GetRenderCmdBuffer      (int index)                 {return swapchain->sc_image[index].cmd_buf;}

public:

            Semaphore *         GetPresentSemaphore     ()                          {return present_complete_semaphore;}

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
