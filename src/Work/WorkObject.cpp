#include<hgl/WorkObject.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/time/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(graph::RenderFramework *rf)
    {
        OnRenderFrameworkChange(rf);
    }

    WorkObject::WorkObject(std::shared_ptr<ecs::ECSContext> ctx)
        : world(std::move(ctx))
    {
        if (world)
        {
            render_context = world->GetRenderContext();
        }
    }

    WorkObject::~WorkObject()
    {
    }

    graph::Camera *WorkObject::GetCamera()
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            return camera_system ? camera_system->GetCamera() : nullptr;
        }

        return nullptr;
    }

    const graph::CameraInfo *WorkObject::GetCameraInfo() const
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            return camera_system ? camera_system->GetCameraInfo() : nullptr;
        }

        return nullptr;
    }

    const VkExtent2D *WorkObject::GetExtent()
    {
        if (world)
        {
            auto target = world->GetRenderTarget();
            return target ? &target->GetExtent() : nullptr;
        }

        return nullptr;
    }

    const graph::ViewportInfo *WorkObject::GetViewportInfo() const
    {
        if (world)
        {
            auto camera_system = world->GetSystem<ecs::CameraSystem>();
            if (camera_system)
                return camera_system->GetViewportInfo();

            auto target = world->GetRenderTarget();
            return target ? target->GetViewportInfo() : nullptr;
        }

        return nullptr;
    }

    void WorkObject::OnRenderFrameworkChange(graph::RenderFramework *rf)
    {
        if(!rf)
        {
            render_framework=nullptr;
            render_context=nullptr;
            world.reset();
            return;
        }

        render_framework=rf;
        world.reset();
        if (rf->GetECSContext())
            world = std::shared_ptr<ecs::ECSContext>(rf->GetECSContext(), [](ecs::ECSContext*){});
        render_context=world?world->GetRenderContext():nullptr;
    }

    void WorkObject::Tick(double delta)
    {
        if (world)
        {
            world->Tick(static_cast<float>(delta));
            return;
        }

    }

    void WorkObject::Render(double delta_time)
    {
        if (world)
            return;
    }

    // Resource helpers removed. Use RenderContext/RenderAPI directly.
}//namespcae hgl
