#pragma once

#include<hgl/WorkObject.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include <memory>

namespace hgl
{
    /**
    * 工作管理器，管理一个序列的WorkObject<br>
    */
    class WorkManager:public io::WindowEvent
    {
    protected:

        AppFramework *app_framework;
        std::unique_ptr<ecs::RenderSystemCore> render_core;

        uint fps=60;
        double frame_time=1.0f/double(fps);

        double last_update_time=0;
        double last_render_time=0;
        double cur_time=0;

        WorkObject *cur_work_object=nullptr;

    public:

        WorkManager(AppFramework *af)
        {
            app_framework=af;

            af->AddChildDispatcher(this);
        }

        explicit WorkManager(std::shared_ptr<ecs::ECSContext> ctx)
        {
            app_framework=nullptr;
            render_core = ctx ? std::make_unique<ecs::RenderSystemCore>(ctx.get()) : nullptr;
            if (render_core)
                render_core->Initialize();
        }

        virtual ~WorkManager()
        {
            if (app_framework)
                app_framework->RemoveChildDispatcher(this);
            if(cur_work_object)
                OnChangeWorkObject(cur_work_object, nullptr);
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

        // Called when the current active WorkObject changes. Default does nothing.
        virtual void OnChangeWorkObject(WorkObject *old_work,WorkObject *new_work);
    };//class WorkManager

    class SwapchainWorkManager:public WorkManager
    {
        graph::SwapchainModule *swapchain_module;

    public:

        SwapchainWorkManager(AppFramework *af):WorkManager(af)
        {
            swapchain_module=af->GetSwapchainModule();
        }

        explicit SwapchainWorkManager(std::shared_ptr<ecs::ECSContext> ctx)
            : WorkManager(std::move(ctx))
        {
            swapchain_module=nullptr;
        }

        ~SwapchainWorkManager()=default;

        void Render(WorkObject *wo) override;

        void OnResize(uint w,uint h) override;
    };

    template<typename WO> int RunFramework(const OSString &title,uint width=1280,uint height=720)
    {
        AppFramework app(title);

        if(!app.Init(width,height))
            return(-1);

        SwapchainWorkManager wm(&app);

        WO *wo=new WO(&app);

        if(!wo->Init())
        {
            delete wo;
            return(-2);
        }

        wm.Run(wo);

        return 0;
    }
}//namespcae hgl
