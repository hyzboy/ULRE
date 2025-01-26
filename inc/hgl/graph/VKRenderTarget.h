#pragma once

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
    VkExtent2D extent;
    
public:

    const   VkExtent2D &GetExtent       ()const{return extent;}

    virtual uint32_t    GetColorCount   ()=0;
    virtual bool        hasDepth        ()=0;

public:

    IRenderTarget(const VkExtent2D &ext){extent=ext;}
    virtual ~IRenderTarget()=default;
    
    virtual Framebuffer *       GetFramebuffer  ()=0;
    virtual RenderPass *        GetRenderPass   ()=0;

    virtual Texture2D *         GetColorTexture (const int index=0)=0;
    virtual Texture2D *         GetDepthTexture ()=0;

public: // Command Buffer

    virtual DeviceQueue *       GetQueue            ()=0;
    virtual Semaphore *         GetRenderCompleteSemaphore()=0;

    virtual RenderCmdBuffer *   GetRenderCmdBuffer  ()=0;

    virtual bool                Submit              (Semaphore *wait_sem)=0;

    virtual bool                Submit              (){return Submit(nullptr);}

    virtual bool                WaitQueue           ()=0;
    virtual bool                WaitFence           ()=0;
};//class IRenderTarget

struct RenderTargetData
{
    Framebuffer *       fbo;
    DeviceQueue *       queue;
    Semaphore *         render_complete_semaphore;

    RenderCmdBuffer *   cmd_buf;

    uint32_t            color_count;            ///<颜色成分数量
    Texture2D **        color_textures;         ///<颜色成分纹理列表
    Texture2D *         depth_texture;          ///<深度成分纹理

public:

    Texture2D *GetColorTexture(const uint32_t index)
    {
        if(index>=color_count)
            return(nullptr);

        return color_textures[index];
    }

    virtual void Clear();
};//struct RenderTargetData

/**
 * 单帧渲染目标
 */
class RenderTarget:public IRenderTarget
{
    RenderTargetData *data;

protected:

    friend class GPUDevice;

    RenderTarget(RenderTargetData *rtd):IRenderTarget(rtd->fbo->GetExtent())
    {
        data=rtd;
    }

public:

    virtual ~RenderTarget()
    {
        if(data)
        {
            data->Clear();
            delete data;
        }
    }

    Framebuffer *       GetFramebuffer      ()override{return data->fbo;}
    RenderPass *        GetRenderPass       ()override{return data->fbo->GetRenderPass();}

    uint32_t            GetColorCount       ()override{return data->color_count;}
    
    bool                hasDepth            ()override{return data->depth_texture;}

    Texture2D *         GetColorTexture     (const int index=0) override{return data->GetColorTexture(index);}
    Texture2D *         GetDepthTexture     ()                  override{return data->depth_texture;}

public: // Command Buffer

    DeviceQueue *       GetQueue            ()override{return data->queue;}
    Semaphore *         GetRenderCompleteSemaphore()override{return data->render_complete_semaphore;}

    RenderCmdBuffer *   GetRenderCmdBuffer  ()override{return data->cmd_buf;}

    virtual bool        Submit              (Semaphore *wait_sem)override
    {
        if(!data)
            return(false);

        return data->queue->Submit(*data->cmd_buf,wait_sem,data->render_complete_semaphore);
    }

    bool                WaitQueue           ()override{return data->queue->WaitQueue();}
    bool                WaitFence           ()override{return data->queue->WaitFence();}
};//class RenderTarget

/**
* 多帧渲染目标
*/
class MFRenderTarget:public IRenderTarget
{
protected:

    uint32_t frame_number;
    uint32_t current_frame;

    RenderTarget **rt_list;

protected:

    friend class GPUDevice;

    MFRenderTarget(const uint32_t fn,RenderTarget **rtl):IRenderTarget(rtl[0]->GetFramebuffer()->GetExtent())
    {
        frame_number=fn;
        current_frame=0;

        rt_list=rtl;
    }

public:

    virtual ~MFRenderTarget()
    {
        SAFE_CLEAR_OBJECT_ARRAY_OBJECT(rt_list,frame_number);
    }

    virtual void        NextFrame()
    {
        ++current_frame;

        if(current_frame>=frame_number)
            current_frame=0;
    }
    
    uint32_t            GetCurrentFrameIndices      ()const{return current_frame;}
    uint32_t            GetFrameCount               ()const{return frame_number;}
    RenderTarget *      GetCurrentFrameRenderTarget (){return rt_list[current_frame];}

public:

    Framebuffer *       GetFramebuffer      ()override{return rt_list[current_frame]->GetFramebuffer();}
    RenderPass *        GetRenderPass       ()override{return rt_list[current_frame]->GetRenderPass();}

    uint32_t            GetColorCount       ()override{return rt_list[current_frame]->GetColorCount();}

    Texture2D *         GetColorTexture     (const int index=0) override{return rt_list[current_frame]->GetColorTexture(index);}
    Texture2D *         GetDepthTexture     ()                  override{return rt_list[current_frame]->GetDepthTexture();}

    
    bool                hasDepth            ()override{return rt_list[current_frame]->hasDepth();}

public: // Command Buffer

    DeviceQueue *       GetQueue            ()override{return rt_list[current_frame]->GetQueue();}
    Semaphore *         GetRenderCompleteSemaphore()override{return rt_list[current_frame]->GetRenderCompleteSemaphore();}
    RenderCmdBuffer *   GetRenderCmdBuffer  ()override{return rt_list[current_frame]->GetRenderCmdBuffer();}
    
    Framebuffer *       GetFramebuffer      (const uint32_t index){return rt_list[index]->GetFramebuffer();}
    RenderCmdBuffer *   GetRenderCmdBuffer  (const uint32_t index){return rt_list[index]->GetRenderCmdBuffer();}

    virtual bool        Submit              ()override{return rt_list[current_frame]->Submit(nullptr);}
    
    virtual bool        Submit              (Semaphore *wait_sem) override
    {
        return rt_list[current_frame]->Submit(wait_sem);
    }

    bool                WaitQueue           ()override{return rt_list[current_frame]->WaitQueue();}
    bool                WaitFence           ()override{return rt_list[current_frame]->WaitFence();}
};//class MFRenderTarget

/**
 * 交换链专用渲染目标
 */
class SwapchainRenderTarget:public MFRenderTarget
{
    VkDevice device;
    Swapchain *swapchain;
    PresentInfo present_info;

    Semaphore *present_complete_semaphore=nullptr;

private:

    SwapchainRenderTarget(VkDevice dev,Swapchain *sc,Semaphore *pcs,RenderTarget **rtl);

    friend class GPUDevice;

public:

    ~SwapchainRenderTarget();

public:

    int     AcquireNextImage();             ///<获取下一帧的索引

    bool    PresentBackbuffer();            ///<推送后台画面到前台

    bool    Submit()override
    {
        return rt_list[current_frame]->Submit(present_complete_semaphore);
    }
};//class RTSwapchain:public RenderTarget
VK_NAMESPACE_END
