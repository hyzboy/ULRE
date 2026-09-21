#pragma once
#include<hgl/framework/WorkManager.h>
#include<hgl/time/Time.h>

namespace hgl
{
    void WorkManager::OnChangeWorkObject(WorkObject *old_work,WorkObject *new_work)
    {
        cur_work_object = new_work;

        (void)app_framework;
    }

    void WorkManager::Tick(WorkObject *wo)
    {
        double delta_time;

        if(last_update_time<=0)
        {
            delta_time=0;
            last_update_time=cur_time;
        }
        else
            delta_time=cur_time-last_update_time;

        if(delta_time>=frame_time)
        {
            last_update_time=cur_time;

            wo->Tick(delta_time);
        }
    }

    void WorkManager::Render(WorkObject *wo)
    {
        // 所有 WorkObject 均经 RunFramework/Qt 外壳注入 ECSContext，
        // 渲染统一走 ECS 托管帧；wo->Render 是帧内 pre_render 回调。
        if (!wo || !wo->GetECSContext())
            return;

        double delta_time;
        bool can_render=wo->IsRenderDirty();

        if(math::IsNearlyZero(last_render_time))
        {
            delta_time=0;
            can_render=true;
        }
        else
        {
            delta_time=cur_time-last_render_time;

            if(!can_render)
                can_render=delta_time>=frame_time;
        }

        if (!can_render)
            return;

        last_render_time=cur_time;
        wo->GetECSContext()->Render(static_cast<float>(delta_time),
                                    [wo](float dt){ wo->Render(static_cast<double>(dt)); });
        wo->ClearRenderDirty();
    }

    void WorkManager::RunFrame(WorkObject *wo)
    {
        cur_time=GetTimeSec();

        Tick(wo);
        Render(wo);
    }

    void WorkManager::Run(WorkObject *wo)
    {
        if(!wo)return;

        OnChangeWorkObject(nullptr,wo);

        if(!cur_work_object)
            return;

        last_update_time=last_render_time=0;

        Window *win=app_framework ? app_framework->GetWindow() : nullptr;
        const bool has_window=win!=nullptr;

        while(!cur_work_object->IsDestroy())
        {
            cur_time=GetTimeSec();

            // 窗口最小化/隐藏时跳过整帧（Tick+Render）
            if(!has_window || win->IsVisible())
                RunFrame(cur_work_object);   // Tick + Render（与外部事件循环驱动共用同一实现）

            if(has_window)
            {
                if(!win->Update())
                    break;
            }
            else
            {
                SleepSecond(0.001);
            }
        }
    }
}//namespace hgl
