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
    void WorkObject::SetECSContext(ecs::ECSContext *ctx)
    {
        world = ctx;
        if (world)
            render_context = world->GetRenderContext();
    }

    void WorkObject::SetClearColor(const Color4f &color)
    {
        // 清屏色唯一权威在渲染目标上（RenderTargetDesc::clear_color）。
        // 写主世界绑定的 RT；world 未就绪时丢弃（Init 前不应设置）。
        if (world)
            if (auto *rt = world->GetRenderTarget())
                rt->SetClearColor(color);
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
            world->Tick(static_cast<float>(delta));
    }

    // WorkObject::OnRenderPass：渲染帧内录制钩子（BeginRenderPass 之后、
    // 系统绘制之前），基类无操作；旧名 Render 保留为 deprecated 别名。

    // Resource helpers removed. Use RenderContext/GraphicsContext directly.
}//namespace hgl
