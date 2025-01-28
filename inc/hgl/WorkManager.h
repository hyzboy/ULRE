#pragma once
#include"WorkObject.h"

namespace hgl
{
    /**
    * 工作管理器，管理一个序列的WorkObject<br>
    */
    class WorkManager
    {
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
        }

        virtual ~WorkManager()
        {
            SAFE_CLEAR(cur_work_object);
        }

        void SetFPS(uint f)
        {
            fps=f;
            frame_time=1.0f/double(fps);
        }

        void Tick(WorkObject *wo);

        virtual void Render(WorkObject *wo);

        void Run(WorkObject *wo);
    };//class WorkManager

    class SwapchainWorkManager:public WorkManager
    {
        graph::SwapchainModule *swpachain_module;

    public:

        SwapchainWorkManager(graph::RenderFramework *rf):WorkManager(rf)
        {
            swpachain_module=rf->GetSwapchainModule();
        }
        ~SwapchainWorkManager()=default;

        void Render(WorkObject *wo) override;
    };
}//namespcae hgl
