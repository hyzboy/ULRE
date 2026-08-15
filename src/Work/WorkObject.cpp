#include<hgl/framework/WorkObject.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/time/Time.h>
//#include<iostream>

namespace hgl
{
    WorkObject::WorkObject(std::shared_ptr<ecs::ECSContext> ctx)
        : world(std::move(ctx))
    {
        if (world)
        {
            render_context = world->GetRenderContext();
        }
    }

    void WorkObject::_InitializeWithECSContext_INTERNAL_DO_NOT_CALL(std::shared_ptr<ecs::ECSContext> ctx)
    {
        world = std::move(ctx);
        if (world)
        {
            render_context = world->GetRenderContext();
        }
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

    const math::Vector2i *WorkObject::GetMouseCoord() const
    {
        if (!world)
            return nullptr;

        auto input_system = world->GetSystem<ecs::InputSystem>();
        return input_system ? &input_system->GetMouseCoord() : nullptr;
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

    // Resource helpers removed. Use RenderContext/GraphicsContext directly.
}//namespcae hgl
