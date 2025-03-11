#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderTargetData.h>

VK_NAMESPACE_BEGIN

/**
* 多帧渲染目标
*/
class MultiFrameRenderTarget:public IRenderTarget
{
protected:

    uint32_t frame_number;
    uint32_t current_frame;

    RenderTargetData *rtd_list;

protected:

    friend class RenderTargetManager;

    MultiFrameRenderTarget(RenderFramework *rf,const uint32_t fn,RenderTargetData *rtl):IRenderTarget(rf,rtl[0].fbo->GetExtent())
    {
        frame_number=fn;
        current_frame=0;

        rtd_list=rtl;
    }

public:

    virtual ~MultiFrameRenderTarget() override;

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

public:

    virtual RenderCmdBuffer *BeginRender()override
    {
        //std::cout<<"Begin Render frame="<<current_frame<<std::endl;
        return rt_list[current_frame]->BeginRender();
    }

    virtual void EndRender() override
    {
        //std::cout<<"End Render frame="<<current_frame<<std::endl;
        rt_list[current_frame]->EndRender();
    }
};//class MultiFrameRenderTarget

VK_NAMESPACE_END
