#pragma once

#include<hgl/WorkObject.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>

namespace hgl
{
    /**
    * 工作管理器，管理一个序列的WorkObject<br>
    */
    class WorkManager:public io::WindowEvent
    {
    protected:

        graph::RenderFramework *render_framework;

        uint fps=60;
        double frame_time=1.0f/double(fps);

        double last_update_time=0;
        double last_render_time=0;
        double cur_time=0;

        WorkObject *cur_work_object=nullptr;

    public:

        WorkManager(graph::RenderFramework *rf)
        {
            render_framework=rf;

            rf->AddChildDispatcher(this);
        }

        virtual ~WorkManager()
        {
            render_framework->RemoveChildDispatcher(this);
            SAFE_CLEAR(cur_work_object);
        }

        void SetFPS(uint f)
        {
            fps=f;
            frame_time=1.0f/double(fps);
        }

                void Tick   (WorkObject *wo);
        virtual void Render (WorkObject *wo);
                void Run    (WorkObject *wo);
    };//class WorkManager

    class SwapchainWorkManager:public WorkManager
    {
        graph::SwapchainModule *swapchain_module;

    public:

        SwapchainWorkManager(graph::RenderFramework *rf):WorkManager(rf)
        {
            swapchain_module=rf->GetSwapchainModule();
        }

        ~SwapchainWorkManager()=default;

        void Render(WorkObject *wo) override;

        void OnResize(uint w,uint h) override;
    };

    template<typename WO> int RunFramework(const OSString &title,uint width=1280,uint height=720)
    {
        graph::RenderFramework rf(title);

        if(!rf.Init(width,height))  
            return(-1);

        SwapchainWorkManager wm(&rf);

        WO *wo=new WO(&rf);

        if(!wo->Init())
        {
            delete wo;
            return(-2);
        }

        wm.Run(wo);

        return 0;
    }
}//namespcae hgl
