#pragma once

#include <hgl/framework/WorkManager.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/InputSystem.h>

#include <memory>

namespace hgl::example
{
    struct OffscreenWorldConfig
    {
        uint32_t width = 512;
        uint32_t height = 512;
        const char *world_name = "OffscreenWorld";
        const char *resource_prefix = "OffscreenRT";
        bool register_input_system = false;
    };

    // Helper for creating and driving an offscreen ECS world bound to a dedicated render target.
    class OffscreenWorldRuntime
    {
    private:
        graph::IRenderTarget *rt_ = nullptr;
        ecs::ECSContext *world_ = nullptr;
        std::unique_ptr<ecs::RenderSystemCore> render_core_;

        std::shared_ptr<ecs::RenderTargetSystem> rt_system_;
        std::shared_ptr<ecs::RenderPrimitiveCollectSystem> collect_system_;
        std::shared_ptr<ecs::RenderDescriptorBindingSystem> descriptor_binding_system_;
        std::shared_ptr<ecs::CameraSystem> camera_system_;

    public:
        ~OffscreenWorldRuntime()
        {
            render_core_.reset();

            if (world_)
            {
                world_->Shutdown();
                delete world_;
                world_ = nullptr;
            }

            delete rt_;
            rt_ = nullptr;
        }

        bool Init(WorkObject *owner, const OffscreenWorldConfig &cfg)
        {
            if (!owner)
                return false;

            ecs::ECSContext *main_world = owner->GetECSContext();
            if (!main_world)
                return false;

            graph::GraphicsContext *gc = main_world->GetGraphicsContext();
            if (!gc)
                return false;

            graph::VulkanDevice *device = gc->GetDevice();
            if (!device)
                return false;

            const graph::VulkanDevAttr *dev_attr = device->GetDevAttr();
            if (!dev_attr)
                return false;

            const VkFormat color_fmt = dev_attr->surface_format.format;
            const VkFormat depth_fmt = dev_attr->physical_device->GetDepthFormat();

            graph::FramebufferInfo fbi(color_fmt, depth_fmt);
            fbi.SetExtent(cfg.width, cfg.height);

            rt_ = graph::RenderTargetManager::CreateRTFromGraphicsContext(gc, main_world, &fbi);
            if (!rt_)
                return false;

            world_ = new ecs::ECSContext(cfg.world_name);
            if (!world_)
                return false;

            world_->SetResourceNamePrefix(cfg.resource_prefix);
            world_->SetRenderContext(owner->GetRenderContext());

            rt_system_ = world_->RegisterRenderSystem<ecs::RenderTargetSystem>();
            collect_system_ = world_->RegisterRenderSystem<ecs::RenderPrimitiveCollectSystem>();
            descriptor_binding_system_ = world_->RegisterRenderSystem<ecs::RenderDescriptorBindingSystem>();
            if (!descriptor_binding_system_)
                return false;

            if (cfg.register_input_system)
                world_->RegisterTickSystem<ecs::InputSystem>();

            camera_system_ = world_->RegisterTickSystem<ecs::CameraSystem>(world_);

            rt_system_->SetRenderContext(owner->GetRenderContext());
            rt_system_->SetRenderTarget(rt_);
            collect_system_->SetWorld(world_);

            // W3 合并后单一入口：GPU 绑定 + 系统初始化（device 来自 :69）
            world_->Initialize(device, rt_);

            if (camera_system_)
            {
                camera_system_->SetRenderContext(owner->GetRenderContext());
                camera_system_->SetViewportInfo(rt_->GetViewportInfo());
                collect_system_->SetCameraInfo(camera_system_->GetCameraInfo());
            }

            render_core_ = std::make_unique<ecs::RenderSystemCore>(world_);
            if (!render_core_ || !render_core_->Initialize())
                return false;

            return true;
        }

        bool RenderOnce(const Color4f &clear_color)
        {
            if (!world_ || !render_core_)
                return false;

            world_->Tick(0.0f);

            render_core_->SetClearColor(clear_color);
            if (!render_core_->BeginFrame())
                return false;

            world_->SetCurrentRenderCmd(render_core_->GetRenderCmd());
            world_->PrepareRenderPassSetup(render_core_->GetSwapchainImageIndex(), 0.0f);

            if (!render_core_->BeginRenderPass())
            {
                world_->SetCurrentRenderCmd(nullptr);
                render_core_->EndFrame();
                return false;
            }

            world_->RenderDrawOnly(render_core_->GetRenderCmd(), 0.0f);
            render_core_->EndFrame();

            world_->SetCurrentRenderCmd(nullptr);
            (void)world_->SubmitFrameToRenderTarget(0.0f);
            return true;
        }

        graph::IRenderTarget *GetRenderTarget() const { return rt_; }
        graph::Texture2D *GetColorTexture(const uint32_t index = 0) const
        {
            return rt_ ? rt_->GetColorTexture(index) : nullptr;
        }
        ecs::ECSContext *GetWorld() const { return world_; }
        std::shared_ptr<ecs::CameraSystem> GetCameraSystem() const { return camera_system_; }
    };
}
