#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/mtl/DescriptorResourceCatalog.h>
#include<cstdlib>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/systems/render/RenderFrameUBOSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKGlobalSceneUBOSet.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<cstdint>
#include<cstring>
#include<unordered_set>
#include<string>

namespace hgl::ecs
{
    namespace
    {
        graph::BufferManager *GetBufferManager(hgl::ecs::ECSContext *ctx)
        {
            if (!ctx)
                return nullptr;

            if (auto *rc = ctx->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetBufferManager();
            }

            if (auto *gc = ctx->GetGraphicsContext())
                return gc->GetBufferManager();

            return nullptr;
        }

        graph::SSBOBufferRegistry *GetSSBOBufferRegistry(hgl::ecs::ECSContext *ctx)
        {
            if (!ctx)
                return nullptr;

            if (auto *rc = ctx->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetSSBOBufferRegistry();
            }

            if (auto *gc = ctx->GetGraphicsContext())
                return gc->GetSSBOBufferRegistry();

            return nullptr;
        }

        graph::GlobalSceneUBOSet *GetGlobalSceneUBOSet(hgl::ecs::ECSContext *ctx)
        {
            if (!ctx)
                return nullptr;

            if (auto *rc = ctx->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetGlobalSceneUBOSet();
            }

            if (auto *gc = ctx->GetGraphicsContext())
                return gc->GetGlobalSceneUBOSet();

            return nullptr;
        }
    }

    RenderSceneUBOSystem::RenderSceneUBOSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameUBOSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();
    }

    RenderSceneUBOSystem::~RenderSceneUBOSystem()
    {
        ReleaseViewportUBO();
    }

    void RenderSceneUBOSystem::EnsureViewportUBO()
    {
        if (viewport_ubo || !context)
            return;

        auto *bm = GetBufferManager(context);
        if (!bm)
            return;

        auto *buf = bm->CreateUBO("ViewportInfoUBO", graph::StructuredBufferAccessor<graph::ViewportInfo>::GetSize());
        if (!buf)
            return;

        buf->SetUpdateClass(graph::BufferUpdateClass::CriticalPerFrame);
        viewport_ubo = graph::StructuredBufferAccessor<graph::ViewportInfo>::Create(buf, &graph::mtl::SBS_ViewportInfo, false);
        if (!viewport_ubo)
            return;

        uint32_t w = pending_viewport_width;
        uint32_t h = pending_viewport_height;
        if (w == 0 && h == 0)
        {
            auto *rt = context->GetRenderTarget();
            if (rt) { w = rt->GetExtent().width; h = rt->GetExtent().height; }
        }
        viewport_ubo->Data()->Set(w, h);
        viewport_ubo->MarkDirty();
    }

    void RenderSceneUBOSystem::ReleaseViewportUBO()
    {
        if (!viewport_ubo)
            return;

        auto *buf = viewport_ubo->ubo();
        delete viewport_ubo;
        viewport_ubo = nullptr;

        if (buf)
        {
            if (auto *bm = GetBufferManager(context))
                bm->Release(buf);
        }
    }

    void RenderSceneUBOSystem::CommitViewportUBO()
    {
        EnsureViewportUBO();
        if (!viewport_ubo)
            return;

        // 视图三件套契约：pass 开始固定写入，extent 取 pending 或当前 RT
        uint32_t w = pending_viewport_width;
        uint32_t h = pending_viewport_height;
        if ((w == 0 && h == 0) && context)
        {
            if (auto *rt = context->GetRenderTarget())
            {
                w = rt->GetExtent().width;
                h = rt->GetExtent().height;
            }
        }

        if (w == 0 || h == 0)
            return;

        graph::ViewportInfo vi{};
        vi.Set(w, h);
        viewport_ubo->Update(vi);    // 拷贝数据 + 置脏
        viewport_ubo->Update();      // 写入 GPU
    }

    graph::ViewportInfo *RenderSceneUBOSystem::GetViewportInfo()
    {
        if (!viewport_ubo)
            EnsureViewportUBO();

        return viewport_ubo ? viewport_ubo->Data() : nullptr;
    }

    void RenderSceneUBOSystem::SetViewportExtent(uint32_t w, uint32_t h)
    {
        pending_viewport_width  = w;
        pending_viewport_height = h;

        if (viewport_ubo)
        {
            viewport_ubo->Data()->Set(w, h);
            viewport_ubo->MarkDirty();
        }
    }

    bool RenderSceneUBOSystem::RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
                                                                     uint32_t ssbo_id,
                                                                     uint32_t byte_stride)
    {
        const uint32_t expected_version = graph::mtl::GetSSBOTypeStructVersion(ssbo_type);
        const uint32_t expected_stride = graph::mtl::GetSSBOTypeStructStride(ssbo_type);
        if (expected_version > 0 && expected_stride > 0 && byte_stride != expected_stride)
        {
            GLogError("[R11] SSBO struct layout rejected: type=%s version=%u expected_stride=%u actual_stride=%u ssbo_id=%u",
                      graph::mtl::GetSSBOTypeName(ssbo_type),
                      expected_version,
                      expected_stride,
                      byte_stride,
                      ssbo_id);
            return false;
        }

        if (auto *domain_manager = GetSSBOBufferRegistry(context))
            domain_manager->Touch(graph::mtl::SSBOAddress{ssbo_type, ssbo_id, 0});

        return true;
    }

    uint32_t RenderSceneUBOSystem::RegisterTextureResource(const std::string &resource_id,
                                                                    graph::Texture *tex,
                                                                    graph::BindlessTextureManager *bindless_mgr)
    {
        if (!tex || !bindless_mgr)
            return 0;

        std::string rid = resource_id;
        if (rid.empty())
            rid = BuildTextureResourceId(tex);

        if (rid.empty())
            return 0;

        const uint32_t handle = bindless_mgr->RegisterTexture(tex);
        if (handle != 0)
            materialization_resource_handles.Add(AnsiString(rid.c_str()), handle);

        return handle;
    }

    uint32_t RenderSceneUBOSystem::GetBindlessHandle(const AnsiString &resource_id) const
    {
        if (resource_id.IsEmpty())
            return 0;

        const uint32_t *handle = materialization_resource_handles.GetValuePointer(resource_id);
        return handle ? *handle : 0;
    }

    void RenderSceneUBOSystem::Update(float /*deltaTime*/)
    {
        SyncBindingsForCurrentCommand();
    }

    void RenderSceneUBOSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        // Critical for RenderDrawOnly path: Update() is not called there.
        SyncBindingsForCurrentCommand();
    }

    void RenderSceneUBOSystem::SyncBindingsForCurrentCommand()
    {
        if (!context)
            return;

        EnsureViewportUBO();

        ApplyResourceLayoutBindings();
    }

    const graph::IGPUBuffer *RenderSceneUBOSystem::ResolveViewportUBO() const
    {
        return viewport_ubo ? viewport_ubo->GetGPUBuffer() : nullptr;
    }

    const graph::IGPUBuffer *RenderSceneUBOSystem::ResolveCameraUBO() const
    {
        if (!context)
            return nullptr;

        auto camera_system = context->GetSystem<CameraSystem>();
        if (!camera_system)
            return nullptr;

        auto *camera_ubo = camera_system->GetCameraUBO();
        if (!camera_ubo)
            return nullptr;

        return camera_ubo->GetGPUBuffer();
    }

    const graph::IGPUBuffer *RenderSceneUBOSystem::ResolveSkyUBO()
    {
        if (!context)
            return nullptr;

        // 环境数据统一归 EnvironmentManager；本 world 的 RT 决定用哪个
        // Profile（未设置 = default）。不再依赖 EnvironmentSystem 的 UBO。
        graph::GraphicsContext *graphics_context = nullptr;
        if (auto *rc = context->GetRenderContext())
            graphics_context = rc->GetGraphicsContext();
        if (!graphics_context)
            graphics_context = context->GetGraphicsContext();
        if (!graphics_context)
            return nullptr;

        auto *env_manager = graphics_context->GetEnvironmentManager();
        if (!env_manager)
            return nullptr;

        graph::EnvProfileID profile_id = graph::kEnvProfileDefault;
        if (auto *rt = context->GetRenderTarget())
            profile_id = rt->GetEnvironmentProfile();

        return env_manager->GetSkyUBO(profile_id);
    }

    // 全局 Scene UBO 描述符集更新：一帧写一次（camera=0/sky=1/viewport=2/palette=3）。
    // camera/viewport 为所有材质必需；sky 与 color_palette 为可选（布局已带
    // PARTIALLY_BOUND 位，未静态使用的 binding 允许为空）。palette 由
    // LineRenderPipeline 等在初始化时写入 binding=3。
    // （绑定时代死段——per-material apply_requirement/MP/批覆盖——已随
    // desc_manager 机制退役整删，2026-09-08。）
    void RenderSceneUBOSystem::ApplyResourceLayoutBindings()
    {
        if (!context)
            return;

        const auto *viewport_ubo = ResolveViewportUBO();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();

        auto *global_scene_set = GetGlobalSceneUBOSet(context);
        if (global_scene_set && global_scene_set->IsValid()
         && viewport_ubo && camera_ubo)
        {
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingCamera),   camera_ubo);
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingViewport), viewport_ubo);
            if (sky_ubo)
                global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingSky), sky_ubo);
        }
        else if (global_scene_set && global_scene_set->IsValid())
        {
            GLogWarning("[RDBinding] Scene UBO set not bound: camera=%p viewport=%p sky=%p",
                        (const void *)viewport_ubo,
                        (const void *)camera_ubo,
                        (const void *)sky_ubo);
        }
    }

}
