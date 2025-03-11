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

    virtual ~MultiFrameRenderTarget() override
    {
        for(uint32_t i=0;i<frame_number;i++)
            rtd_list[i].Clear();

        delete[] rtd_list;
    }

    virtual bool NextFrame()
    {
        ++current_frame;

        if(current_frame>=frame_number)
            current_frame=0;

        return(true);
    }

    uint32_t            GetCurrentFrameIndices      ()const{return current_frame;}
    uint32_t            GetFrameCount               ()const{return frame_number;}

public:

    Framebuffer *       GetFramebuffer              ()override{return rtd_list[current_frame].fbo;}
    RenderPass *        GetRenderPass               ()override{return rtd_list[current_frame].fbo->GetRenderPass();}

    uint32_t            GetColorCount               ()override{return rtd_list[current_frame].color_count;}

    Texture2D *         GetColorTexture             (const int index=0) override{return rtd_list[current_frame].GetColorTexture(index);}
    Texture2D *         GetDepthTexture             ()                  override{return rtd_list[current_frame].depth_texture;}


    bool                hasDepth                    ()override{return rtd_list[current_frame].depth_texture;}

public: // Command Buffer

    DeviceQueue *       GetQueue                    ()override{return rtd_list[current_frame].queue;}
    Semaphore *         GetRenderCompleteSemaphore  ()override{return rtd_list[current_frame].render_complete_semaphore;}
    RenderCmdBuffer *   GetRenderCmdBuffer          ()override{return rtd_list[current_frame].cmd_buf;}

    Framebuffer *       GetFramebuffer              (const uint32_t index){return rtd_list[index].fbo;}
    RenderCmdBuffer *   GetRenderCmdBuffer          (const uint32_t index){return rtd_list[index].cmd_buf;}

    virtual bool        Submit                      (Semaphore *wait_sem)override{return rtd_list[current_frame].Submit(wait_sem);}

    bool                WaitQueue                   ()override{return rtd_list[current_frame].queue->WaitQueue();}
    bool                WaitFence                   ()override{return rtd_list[current_frame].queue->WaitFence();}

public:

    virtual RenderCmdBuffer *BeginRender()override
    {
        //std::cout<<"Begin Render frame="<<current_frame<<std::endl;
        return rtd_list[current_frame].BeginRender(GetDescriptorBinding());
    }

    virtual void EndRender() override
    {
        //std::cout<<"End Render frame="<<current_frame<<std::endl;
        rtd_list[current_frame].EndRender();
    }
};//class MultiFrameRenderTarget

VK_NAMESPACE_END
