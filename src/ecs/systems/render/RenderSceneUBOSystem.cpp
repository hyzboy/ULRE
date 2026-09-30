#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/vk/buffer/StructView.h>
#include<cstdlib>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
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

    }

    RenderSceneUBOSystem::RenderSceneUBOSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
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

        // BDA：viewport 数据自 S2 起经 global_addresses.addr_viewport 解引用（单份 buffer ⇒
        // 全帧槽同址），不再吃 Scene 集绑定 ⇒ 必须以带 SHADER_DEVICE_ADDRESS usage 的方式创建
        //（只有 CreateSSBO 带该 usage）。
        auto *buf = bm->CreateSSBO("ViewportInfoUBO", graph::StructView<graph::ViewportInfo>::GetSize());
        if (!buf)
            return;

        buf->SetUpdateClass(graph::BufferUpdateClass::CriticalPerFrame);
        viewport_ubo = graph::StructView<graph::ViewportInfo>::Create(buf, false);
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

        // 地址进表：**在物化处注册**（地址唯一会变的地方，取不到即 fail-fast）。viewport 是
        // 单份 buffer（内容按 pass/RT 覆盖写、地址恒定）⇒ 一个地址写满所有帧槽。
        graph::GraphicsContext *gc = nullptr;
        if (auto *rc = context->GetRenderContext())
            gc = rc->GetGraphicsContext();
        if (!gc)
            gc = context->GetGraphicsContext();

        auto *registry = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        if (!registry)
        {
            GLogError("[SceneUBO] viewport 地址入表失败：GlobalSSBOBufferRegistry 不可用");
            ReleaseViewportUBO();
            return;
        }

        const uint64_t viewport_addr =
            gc->GetDevice()->GetBufferDeviceAddressAligned16(buf->GetBuffer());
        if (viewport_addr == 0)
        {
            // 取不到地址 = shader 解引用 0 基址（UB）⇒ fail-fast（0 校验层消息抓不到这类崩）
            GLogError("[SceneUBO] viewport 取不到设备地址（usage / 16B 对齐）");
            ReleaseViewportUBO();
            return;
        }

        registry->SetViewportAddress(viewport_addr);
        GLogInfo("[SceneUBO] viewport addr=0x%llX 入表（全帧槽）",
                 (unsigned long long)viewport_addr);
    }

    void RenderSceneUBOSystem::ReleaseViewportUBO()
    {
        if (!viewport_ubo)
            return;

        auto *buf = viewport_ubo->GetBuffer();
        delete viewport_ubo;
        viewport_ubo = nullptr;

        if (buf)
        {
            if (auto *bm = GetBufferManager(context))
                bm->Release(buf);
        }
    }

    // 全局地址表（SSBO，经 pc_root.addr_global_addresses 寻址）里已不再有渲染项 / 绘制项字段：
    // 它们是**世界私有** buffer 的地址，随世界表（WorldAddresses）下发 —— 见
    // doc/world-addresses-and-camera-model-plan.md §1 与 ECSContext::SyncWorldAddresses()。

    void RenderSceneUBOSystem::CommitViewportUBO()
    {
        EnsureViewportUBO();
        if (!viewport_ubo)
            return;

        if (context)
        {
            if (auto *rt = context->GetRenderTarget())
            {
                if (const auto *vi = rt->GetViewportInfo())
                {
                    viewport_ubo->Update(*vi);
                    viewport_ubo->Commit();
                    return;
                }
            }
        }

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
        viewport_ubo->Commit();      // 标脏交 L2（上传/直写由 RenderBufferUploadSystem 统一处理）
    }

    graph::ViewportInfo *RenderSceneUBOSystem::GetViewportInfo()
    {
        if (context)
        {
            if (auto *rt = context->GetRenderTarget())
                return rt->GetViewportInfo();
        }

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

    bool RenderSceneUBOSystem::RegisterMaterialStructLayout(graph::GlobalSSBOType ssbo_type,
                                                                     uint32_t ssbo_id,
                                                                     uint32_t byte_stride)
    {
        const uint32_t expected_version = graph::GetGlobalSSBOTypeStructVersion(ssbo_type);
        const uint32_t expected_stride = graph::GetGlobalSSBOTypeStructStride(ssbo_type);
        if (expected_version > 0 && expected_stride > 0 && byte_stride != expected_stride)
        {
            GLogError("[R11] SSBO struct layout rejected: type=%s version=%u expected_stride=%u actual_stride=%u ssbo_id=%u",
                      graph::GetGlobalSSBOTypeName(ssbo_type),
                      expected_version,
                      expected_stride,
                      byte_stride,
                      ssbo_id);
            return false;
        }

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

    // 全局 Scene UBO 描述符集更新：一帧写一次（sky=0/viewport=1/shadow=2）。
    // 全局地址表已 BDA 化（表本体 SSBO，基址经 pc_root）——不在本集内。
    // viewport 为所有材质必需；sky/shadow 为可选（布局已带 PARTIALLY_BOUND 位，
    // 未静态使用的 binding 允许为空）。调色板已 BDA 化，不在本集内。
    // （绑定时代死段——per-material apply_requirement/MP/批覆盖——已随
    // desc_manager 机制退役整删，2026-09-08。）
    void RenderSceneUBOSystem::ApplyResourceLayoutBindings()
    {
        if (!context)
            return;

        // 全局地址表已 BDA 化（无绑定无集）：Scene 集已于 S3 整体退场，这里只同步表内会变的字段。
        // sky / viewport / shadow 的地址都在表内，shader 经 global_addresses 解引用 ⇒ 无绑定可推。
        // 渲染项 / DrawItemID 的地址现由**世界表**（WorldAddresses）承载，随
        // ECSContext::SetFrameIndex 写入本帧槽 ⇒ 全局表内不再有这两个字段，无绑定可推。
    }

}
