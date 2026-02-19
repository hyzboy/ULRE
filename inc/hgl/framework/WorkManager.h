#pragma once

#include<hgl/framework/WorkObject.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/framework/AppFramework.h>
#include<hgl/object/ObjectTracker.h>
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

    template<typename WO> int RunFramework(const OSString &title,uint width=1280,uint height=720)
    {
        hgl::utils::initialize_object_tracker();

        int result = 0;

        // Use explicit scope to ensure AppFramework/WorkObject destructors are called
        // BEFORE shutdown_object_tracker() checks for leaks
        {
            AppFramework app(title);

            if(!app.Init(width,height))
            {
                result = -1;
            }
            else
            {
                WorkManager wm(&app);

                std::shared_ptr<ecs::ECSContext> world;
                if (app.GetECSContext())
                    world = std::shared_ptr<ecs::ECSContext>(app.GetECSContext(), [](ecs::ECSContext*){});

                WO *wo=new WO();
                wo->_InitializeWithECSContext_INTERNAL_DO_NOT_CALL(world);

                if(!wo->Init())
                {
                    delete wo;
                    result = -2;
                }
                else
                {
                    wm.Run(wo);
                }
            }
        } // AppFramework destructor called here (cleanup happens)

        // Now check for leaks AFTER all destructors have run
        hgl::utils::shutdown_object_tracker();

        return result;
    }
}//namespcae hgl
