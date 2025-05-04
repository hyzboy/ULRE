#pragma once
#include<hgl/WorkManager.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>

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

        if(delta_time>=frame_time||wo->IsRenderDirty())
        {
            last_render_time=cur_time;
            wo->Render(delta_time);
        }
    }

    void SwapchainWorkManager::OnResize(uint w,uint h)
    {
        if(!cur_work_object)return;

        VkExtent2D ext={w,h};

        graph::IRenderTarget *rt=render_framework->GetSwapchainRenderTarget();

        cur_work_object->OnRenderTargetSwitch(render_framework,rt);
        cur_work_object->OnResize(ext);
    }

    void SwapchainWorkManager::Render(WorkObject *wo)
    {
        if(!swapchain_module->AcquireNextImage())
            return;

        graph::SwapchainRenderTarget *rt=swapchain_module->GetRenderTarget();

        wo->MarkRenderDirty();      //临时的，未来会被更好的机制替代
        WorkManager::Render(wo);

        if(!rt)
            return;

        rt->WaitQueue();
        rt->WaitFence();
    }

    void WorkManager::Run(WorkObject *wo)
    {
        if(!wo)return;

        last_update_time=last_render_time=0;

        cur_work_object=wo;

        wo->OnRenderTargetSwitch(render_framework,render_framework->GetSwapchainRenderTarget());

        Window *win=render_framework->GetWindow();
        graph::GPUDevice *dev=render_framework->GetDevice();

        while(!cur_work_object->IsDestroy())
        {
            cur_time=GetPreciseTime();

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
}//namespcae hgl
