#include<hgl/framework/WorkManager.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/time/Time.h>

namespace hgl
{
    WorkManager::~WorkManager()
    {
        // WorkObject 所有权归创建方（RunFramework 的 delete wo / Qt 外壳），此处只解引用。
        cur_work_object = nullptr;
    }

    void WorkManager::RunFrame(WorkObject *wo)
    {
        if (!wo)
            return;

        if (!wo->GetECSContext())
            return;

        cur_time = GetTimeSec();

        // 更新节流：首帧 delta 为 0，其后按实际间隔累计。
        double delta_time;

        if (last_frame_time < 0)
        {
            delta_time = 0;
            last_frame_time = cur_time;
        }
        else
            delta_time = cur_time - last_frame_time;

        if (delta_time >= frame_time)
        {
            last_frame_time = cur_time;

            // wo->Tick 默认转发 ECSContext::Tick；子类覆写后须尾调基类。
            wo->Tick(delta_time);

            wo->GetECSContext()->Render(static_cast<float>(delta_time),
                                        [wo](float dt){ wo->OnRenderPass(static_cast<double>(dt)); });
        }
    }

    void WorkManager::Run(WorkObject *wo)
    {
        if (!wo)
            return;

        cur_work_object = wo;
        last_frame_time = -1;

        Window *win = app_framework ? app_framework->GetWindow() : nullptr;
        const bool has_window = win != nullptr;

        while (!cur_work_object->IsDestroy())
        {
            // 窗口最小化/隐藏时跳过整帧
            if (!has_window || win->IsVisible())
                RunFrame(cur_work_object);

            if (has_window)
            {
                if (!win->Update())
                    break;
            }
            else
                SleepSecond(0.001);
        }

        cur_work_object = nullptr;
    }

    void EnableObjectTracking()
    {
        hgl::utils::initialize_object_tracker();
    }

    void DisableObjectTracking()
    {
        hgl::utils::shutdown_object_tracker();
    }
}//namespace hgl