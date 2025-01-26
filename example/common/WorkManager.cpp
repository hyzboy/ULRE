#pragma once
#include"WorkManager.h"

namespace hgl
{
    void WorkManager::Tick(WorkObject *wo)
    {
        double delta_time=cur_time-last_update_time;

        if(delta_time>=frame_time)
        {
            last_update_time=cur_time;
            wo->Tick(delta_time);
        }
    }

    void WorkManager::Render(WorkObject *wo)
    {
        double delta_time=cur_time-last_render_time;

        if(delta_time>=frame_time)
        {
            last_render_time=cur_time;
            wo->Render(delta_time);
        }
    }

    void WorkManager::Run()
    {
        if(!cur_work_object)
            return;

        Window *win=render_framework->GetWindow();
        graph::GPUDevice *dev=render_framework->GetDevice();

        while(!cur_work_object->IsDestroy())
        {
            cur_time=GetDoubleTime();

            if(cur_work_object->IsTickable())
                Tick(cur_work_object);

            if(win->IsVisible()&&cur_work_object->IsRenderable())
            {
                Render(cur_work_object);
                dev->WaitIdle();
            }

            if(!win->Update())
                break;
        }
    }

    void WorkManager::Start(WorkObject *wo)
    {
        if(!wo)return;

        last_update_time=last_render_time=0;

        cur_work_object=wo;

        wo->Join(render_framework);

        Run();
    }
}//namespcae hgl
