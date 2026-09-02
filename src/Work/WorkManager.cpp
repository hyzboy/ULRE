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
        double delta_time;
        bool can_render=wo->IsRenderDirty();

        if(math::IsNearlyZero(last_render_time))
        {
            delta_time=0;
            can_render=true;
        }
        else
        {
            if(last_render_time<=0)
            {
                delta_time=0;
                last_render_time=cur_time;
            }
            else
                delta_time=cur_time-last_render_time;

            if(!can_render)
                can_render=delta_time>=frame_time;
        }

        if (wo && wo->GetECSContext())
        {
            if (!can_render)
                return;

            last_render_time=cur_time;
            wo->GetECSContext()->SetClearColor(wo->GetClearColor());
            wo->GetECSContext()->Render(static_cast<float>(delta_time),
                                        [wo](float dt){ wo->Render(static_cast<double>(dt)); });
            wo->ClearRenderDirty();
            return;
        }

        if(can_render)
        {
            last_render_time=cur_time;
            wo->Render(delta_time);
        }
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
        graph::VulkanDevice *dev=app_framework ? app_framework->GetDevice() : wo->GetDevice();
        const bool has_window=win!=nullptr;

        while(!cur_work_object->IsDestroy())
        {
            cur_time=GetTimeSec();

            if (app_framework)
                app_framework->Tick();

            if(cur_work_object->IsTickable())
                Tick(cur_work_object);

            if(has_window?win->IsVisible():true)//&&cur_work_object->IsRenderable())
            {
                Render(cur_work_object);
            }

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
}//namespcae hgl
