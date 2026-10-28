#include<hgl/graph/module/OffscreenWorld.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/asset/TextureURI.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    namespace
    {
        /// 连续多少帧资源未就绪后打印一次告警（避免每帧刷屏，又不至于静默失败）
        constexpr uint32_t kPendingWarnFrames = 120;
    }

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
        // 顺序敏感（三段，缺一段就会踩悬垂指针）：
        // 1. 本类持有的四个系统 shared_ptr 必须在 ECSContext 存活时先放掉。
        //    `ecs::System::context` 是裸指针，若 ECSContext 已销毁而这里还持有最后一个
        //    引用，系统析构路径（如 ~RenderSceneUBOSystem → ReleaseViewportUBO →
        //    GetBufferManager(context) → context->GetRenderContext()）会解引用悬垂 context：
        //    RenderToTexture 系列实测 AV @ 0xFEEEFEEE。
        //    放掉后系统仍由 ECSContext 自身的 shared_ptr 持有，最终在 Shutdown 内销毁，
        //    那时 context 仍是有效对象。
        camera_system_.reset();
        ubo_system_.reset();
        collect_system_.reset();
        rt_system_.reset();

        // 2. ECSContext 再 Shutdown（其 RenderSystemCore 持有 RT 指针），再释放
        if(world_)
        {
            world_->Shutdown();
            world_.reset();
        }

        // 3. RT 最后释放
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
        cull_mode_  = desc.cull_mode;

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
        RenderTargetDesc rt_desc = desc.depth_only
                                       ? RenderTargetDesc::OffscreenDepthOnly(desc.width,
                                                                             desc.height,
                                                                             desc.resource_prefix.c_str(),
                                                                             desc.depth_format)
                                       : RenderTargetDesc::OffscreenColorDepth(desc.width,
                                                                              desc.height,
                                                                              desc.resource_prefix.c_str(),
                                                                              desc.depth_format);

        rt_desc.clear_color = desc.clear_color;
        rt_desc.env_profile = desc.env_profile;

        // RT 名 = rt: 寻址的键。必须在 Create 之前取，创建后 desc 不再被读。
        rt_name_ = FromRenderTargetName(rt_desc.name);

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

        rt_system_      = world_->RegisterSystem<ecs::RenderTargetSystem>();
        collect_system_ = world_->RegisterSystem<ecs::RenderPrimitiveCollectSystem>();
        ubo_system_     = world_->RegisterSystem<ecs::RenderSceneUBOSystem>();

        if(!ubo_system_)
        {
            GLogError("[OffscreenWorld] register RenderSceneUBOSystem failed");
            return false;
        }

        if(desc.register_input_system)
            world_->RegisterSystem<ecs::InputSystem>();

        if(desc.register_camera_system)
            camera_system_ = world_->EnsureCameraSystem();

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

        // **绘制前**判定资源就绪：本帧若现在提交，画出来的是不是完整内容。
        // 必须在 Tick/RenderTo 之前取，否则可能被本帧 DriveResources 改过状态。
        const bool scene_ready = !world_->HasPendingRenderableResources();

        world_->Tick(delta_time);

        // 复用与主窗口路径相同的帧驱动（BeginManagedRenderFrame + RenderDrawOnly + EndManagedRenderFrame）
        world_->RenderTo(rt_.get(), clear_color, delta_time, cull_mode_);

        RestoreMainRenderContext();

        // **资源就绪 ≠ 内容已画出**：资源刚 Resident 的那一帧，图元可能还卡在材质物化链
        // （`materialBatches` 为空 ⇒ 一个 DrawCall 都没有），此时若置就绪位，下游会把
        // 「只有清屏色」当内容采样，而离屏侧随后停止驱动 ⇒ 内容永远补不上（离屏示例实测：
        // 主场景看到的是纯清屏色）。所以判据必须再看本帧**真建出了批**。
        // 没有图元进收集（空场景 / 全被剔除）不算残缺 —— 那种情况下清屏色就是内容。
        const auto &frame_cache = world_->GetRenderFrameCache();
        const bool drew_content = frame_cache.renderItems.empty()
                                ? true
                                : frame_cache.materialBatches.GetCount() > 0;

        if(scene_ready && drew_content)
        {
            MarkContentReady();
            return;
        }

        // 内容还不完整：本帧内容残缺，不置就绪位。下游材质因此继续被 CanRender 拦住，
        // 不会把「只有清屏色 / 少了贴图」的画面采样出去。
        ++pending_frames_;

        if(pending_frames_ == kPendingWarnFrames)
            GLogWarning("[OffscreenWorld] 已连续 %u 帧离屏内容不可用（资源就绪=%d，本帧建批=%u），输出暂不可用",
                        pending_frames_,
                        scene_ready ? 1 : 0,
                        (uint32_t)frame_cache.materialBatches.GetCount());
    }

    bool OffscreenWorld::IsSceneContentReady() const
    {
        return world_ ? !world_->HasPendingRenderableResources() : true;
    }

    void OffscreenWorld::MarkContentReady()
    {
        if(content_ready_)
            return;

        content_ready_  = true;
        pending_frames_ = 0;

        // 把就绪位传播到 rt:<name>:<attachment> 句柄 —— 这是下游材质看到的唯一状态源。
        if(TextureManager *tm = gc_ ? gc_->GetTextureManager() : nullptr)
        {
            if(rt_name_.IsEmpty())
                return;

            const int count = tm->MarkRenderTargetContentReady(rt_name_);

            GLogInfo(OS_TEXT("[OffscreenWorld] 离屏输出就绪（rt:%s，%d 个句柄）"),
                     rt_name_.c_str(),
                     count);
        }
    }

    bool OffscreenWorld::Resize(const uint32_t width, const uint32_t height)
    {
        if(!gc_ || !rt_)
            return false;

        auto *rtm = gc_->GetRenderTargetManager();
        if(!rtm)
            return false;

        if(!rtm->Resize(rt_.get(), width, height))
            return false;

        // RT 重建 ⇒ 附件纹理换了，内容回到"未产出"。就绪位在重新画过一帧后由
        // Render() 自动置回，下游材质在此期间被 CanRender 拦住。
        content_ready_  = false;
        pending_frames_ = 0;

        // RT 重建后视口信息随之变化，需重新接线相机与收集系统
        IRenderTarget *rt = rt_.get();

        if(rt_system_)
            rt_system_->SetRenderTarget(rt);

        if(camera_system_)
            camera_system_->SetViewportInfo(rt->GetViewportInfo());

        if(world_)
            world_->SetRenderTarget(rt);

        return true;
    }

    void OffscreenWorld::RestoreMainRenderContext()
    {
        // 原"恢复 RenderContext::current_render_target"已随 RenderContext 双状态
        // 删除而不再需要：主世界的渲染目标权威在主世界 ECSContext::render_target，
        // 子世界 Render() 期间的临时切换由 ECSContext::RenderTo 内部保存/恢复，
        // 不会外泄到主世界。
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
