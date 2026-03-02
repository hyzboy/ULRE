#pragma once

#include<hgl/framework/WorkObject.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/framework/AppFramework.h>
#include<hgl/object/ObjectTracker.h>
#include <memory>
#include <string_view>

namespace hgl
{
    inline bool ParseShaderGenPathModeFromArgs(int argc, os_char **argv, graph::ShaderGenPathMode &out_mode)
    {
        if (!argv || argc <= 1)
            return false;

        constexpr std::basic_string_view<os_char> kOption = OS_TEXT("--shadergen-path-mode");
        constexpr std::basic_string_view<os_char> kLegacy = OS_TEXT("legacy-only");
        constexpr std::basic_string_view<os_char> kValidate = OS_TEXT("mirror-validate");
        constexpr std::basic_string_view<os_char> kPreferred = OS_TEXT("mirror-preferred");

        for (int i = 1; i < argc; ++i)
        {
            const os_char *arg = argv[i];
            if (!arg || !arg[0])
                continue;

            std::basic_string_view<os_char> arg_view(arg);

            if (arg_view == kOption)
            {
                if (i + 1 >= argc || !argv[i + 1])
                    continue;

                std::basic_string_view<os_char> value_view(argv[i + 1]);
                if (value_view == kLegacy)
                {
                    out_mode = graph::ShaderGenPathMode::LegacyOnly;
                    return true;
                }
                if (value_view == kValidate)
                {
                    out_mode = graph::ShaderGenPathMode::MirrorValidate;
                    return true;
                }
                if (value_view == kPreferred)
                {
                    out_mode = graph::ShaderGenPathMode::MirrorPreferred;
                    return true;
                }

                ++i;
                continue;
            }

            if (arg_view.size() > kOption.size() && arg_view.substr(0, kOption.size()) == kOption && arg_view[kOption.size()] == static_cast<os_char>('='))
            {
                std::basic_string_view<os_char> value_view = arg_view.substr(kOption.size() + 1);
                if (value_view == kLegacy)
                {
                    out_mode = graph::ShaderGenPathMode::LegacyOnly;
                    return true;
                }
                if (value_view == kValidate)
                {
                    out_mode = graph::ShaderGenPathMode::MirrorValidate;
                    return true;
                }
                if (value_view == kPreferred)
                {
                    out_mode = graph::ShaderGenPathMode::MirrorPreferred;
                    return true;
                }
            }
        }

        return false;
    }

    /**
    * 工作管理器，管理一个序列的WorkObject<br>
    */
    class WorkManager:public io::WindowEvent
    {
    protected:

        AppFramework *app_framework;
        uint fps=60;
        double frame_time=1.0f/double(fps);

        double last_update_time=-1;
        double last_render_time=-1;
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
        return RunFramework<WO>(title,0,nullptr,width,height);
    }

    template<typename WO> int RunFramework(const OSString &title,int argc,os_char **argv,uint width=1280,uint height=720)
    {
        hgl::utils::initialize_object_tracker();

        int result = 0;

        // Use explicit scope to ensure AppFramework/WorkObject destructors are called
        // BEFORE shutdown_object_tracker() checks for leaks
        {
            AppFramework app(title);

            graph::ShaderGenPathMode parsed_mode;
            if(ParseShaderGenPathModeFromArgs(argc,argv,parsed_mode))
                app.SetShaderGenPathMode(parsed_mode);

            if(!app.Init(width,height,argc,argv))
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
