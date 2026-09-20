#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    std::unique_ptr<OffscreenWorld> OffscreenWorld::Create(GraphicsContext *gc,
                                                           ecs::ECSContext *main_world,
                                                           const OffscreenWorldDesc &desc)
    {
        auto world = std::unique_ptr<OffscreenWorld>(new OffscreenWorld());

        if(!world->Init(gc, main_world, desc))
            return nullptr;

        return world;
    }

    OffscreenWorld::~OffscreenWorld()
    {
        // 顺序敏感：ECSContext 先 Shutdown（其 RenderSystemCore 持有 RT 指针），再释放 RT
        if(world_)
        {
            world_->Shutdown();
            world_.reset();
        }

        rt_.reset();
    }

    bool OffscreenWorld::Init(GraphicsContext *gc, ecs::ECSContext *main_world, const OffscreenWorldDesc &desc)
    {
        if(!gc)
        {
            GLogError("[OffscreenWorld] GraphicsContext is null");
            return false;
        }

        gc_         = gc;
        main_world_ = main_world;

        if(main_world_)
            render_context_ = main_world_->GetRenderContext();

        VulkanDevice *device = gc->GetDevice();
        RenderTargetManager *rtm = gc->GetRenderTargetManager();

        if(!device || !rtm)
        {
            GLogError("[OffscreenWorld] device or RenderTargetManager is null");
            return false;
        }

        // 声明式创建离屏 RT：颜色/深度格式的默认值由 Create() 按设备解析
        RenderTargetDesc rt_desc = RenderTargetDesc::OffscreenColorDepth(
                                        desc.width,
                                        desc.height,
                                        desc.resource_prefix.c_str());

        rt_desc.clear_color = desc.clear_color;
        rt_desc.env_profile = desc.env_profile;

        rt_ = rtm->Create(rt_desc);

        if(!rt_)
        {
            GLogError("[OffscreenWorld] create render target failed (%ux%u)", desc.width, desc.height);
            return false;
        }

        world_ = std::make_unique<ecs::ECSContext>(desc.name);

        if(!world_)
            return false;

        world_->SetResourceNamePrefix(desc.resource_prefix);

        if(render_context_)
            world_->SetRenderContext(render_context_);

        rt_system_      = world_->RegisterRenderSystem<ecs::RenderTargetSystem>();
        collect_system_ = world_->RegisterRenderSystem<ecs::RenderPrimitiveCollectSystem>();
        ubo_system_     = world_->RegisterRenderSystem<ecs::RenderSceneUBOSystem>();

        if(!ubo_system_)
        {
            GLogError("[OffscreenWorld] register RenderSceneUBOSystem failed");
            return false;
        }

        if(desc.register_input_system)
            world_->RegisterTickSystem<ecs::InputSystem>();

        if(desc.register_camera_system)
            camera_system_ = world_->RegisterTickSystem<ecs::CameraSystem>(world_.get());

        IRenderTarget *rt = rt_.get();

        if(rt_system_)
        {
            rt_system_->SetRenderContext(render_context_);
            rt_system_->SetRenderTarget(rt);
        }

        if(collect_system_)
            collect_system_->SetWorld(world_.get());

        // W3 合并后单一入口：GPU 绑定 + 系统初始化
        world_->Initialize(device, rt);

        if(camera_system_)
        {
            camera_system_->SetRenderContext(render_context_);
            camera_system_->SetViewportInfo(rt->GetViewportInfo());

            if(collect_system_)
                collect_system_->SetCameraInfo(camera_system_->GetCameraInfo());
        }

        return true;
    }

    void OffscreenWorld::Render(float delta_time)
    {
        if(!rt_)
            return;

        // 清屏色以 RT 上声明的值为准
        Render(rt_->GetClearColor(), delta_time);
    }

    void OffscreenWorld::Render(const Color4f &clear_color, float delta_time)
    {
        if(!world_ || !rt_)
            return;

        world_->Tick(delta_time);

        // 复用与主窗口路径相同的帧驱动（BeginManagedRenderFrame + RenderDrawOnly + EndManagedRenderFrame）
        world_->RenderTo(rt_.get(), clear_color, delta_time);

        RestoreMainRenderContext();
    }

    void OffscreenWorld::RestoreMainRenderContext()
    {
        if(!render_context_ || !main_world_)
            return;

        // 子世界的 RenderTargetSystem 在 RenderPreBeginFrame 相位会把
        // RenderContext 的 current RT 改写为本世界的离屏 RT。主世界下一帧
        // 若直接用它，会渲染到错误目标；这里显式恢复为主世界的 RT。
        render_context_->SetCurrentRenderTarget(main_world_->GetRenderTarget());
    }

    Texture2D *OffscreenWorld::GetColorTexture(const uint32_t index)const
    {
        return rt_ ? rt_->GetColorTexture(static_cast<int>(index)) : nullptr;
    }

    Texture2D *OffscreenWorld::GetDepthTexture()const
    {
        return rt_ ? rt_->GetDepthTexture() : nullptr;
    }

}//namespace hgl::graph
