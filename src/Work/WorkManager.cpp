#pragma once
#include<hgl/WorkManager.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>

namespace hgl
{
    void WorkManager::OnChangeWorkObject(WorkObject *old_work,WorkObject *new_work)
    {
        cur_work_object = new_work;

        if(cur_work_object)
        {
            if (render_framework)
            {
                // Notify change of active work object
                cur_work_object->OnRenderFrameworkChange(render_framework);
            }

            if (!render_core)
            {
                auto ctx = cur_work_object->GetECSContext();
                if (ctx)
                {
                    render_core = std::make_unique<ecs::RenderSystemCore>(ctx);
                    render_core->Initialize();
                }
            }
        }
        else
        {

        }
    }

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

        if (render_core && wo && wo->GetECSContext())
        {
            if (!can_render)
                return;

            render_core->SetClearColor(wo->GetClearColor());

            if (!render_core->BeginFrame())
                return;

            last_render_time=cur_time;
            wo->Render(delta_time);
            wo->GetECSContext()->Render(render_core->GetRenderCmd(), static_cast<float>(delta_time));
            render_core->EndFrame();
            wo->ClearRenderDirty();
            return;
        }

        if(can_render)
        {
            last_render_time=cur_time;
            wo->Render(delta_time);
        }
    }

    void SwapchainWorkManager::OnResize(uint w,uint h)
    {
        if(!cur_work_object)return;

        if (!render_framework)
            return;

        VkExtent2D ext={w,h};

        cur_work_object->OnRenderFrameworkChange(render_framework);
        cur_work_object->OnResize(ext);
    }

    void SwapchainWorkManager::Render(WorkObject *wo)
    {
        if (render_core && wo && wo->GetECSContext())
        {
            WorkManager::Render(wo);
            return;
        }

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

        OnChangeWorkObject(nullptr,wo);

        if(!cur_work_object)
            return;

        last_update_time=last_render_time=0;

        Window *win=render_framework ? render_framework->GetWindow() : nullptr;
        graph::VulkanDevice *dev=render_framework ? render_framework->GetDevice() : wo->GetDevice();
        const bool has_window=win!=nullptr;

        while(!cur_work_object->IsDestroy())
        {
            cur_time=GetTimeSec();

            if (render_framework)
                render_framework->Tick();

            if(cur_work_object->IsTickable())
                Tick(cur_work_object);

            if(has_window?win->IsVisible():true)//&&cur_work_object->IsRenderable())
            {
                Render(cur_work_object);
                if (dev)
                    dev->WaitIdle();
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
