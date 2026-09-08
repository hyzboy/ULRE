#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
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
#include<hgl/vk/VKMaterialParameters.h>
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

    RenderDescriptorBindingSystem::RenderDescriptorBindingSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameUBOSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();
    }

    RenderDescriptorBindingSystem::~RenderDescriptorBindingSystem()
    {
        ReleaseViewportUBO();
    }

    void RenderDescriptorBindingSystem::EnsureViewportUBO()
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

    void RenderDescriptorBindingSystem::ReleaseViewportUBO()
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

    void RenderDescriptorBindingSystem::CommitViewportUBO()
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

    graph::ViewportInfo *RenderDescriptorBindingSystem::GetViewportInfo()
    {
        if (!viewport_ubo)
            EnsureViewportUBO();
        return viewport_ubo ? viewport_ubo->Data() : nullptr;
    }

    void RenderDescriptorBindingSystem::SetViewportExtent(uint32_t w, uint32_t h)
    {
        pending_viewport_width  = w;
        pending_viewport_height = h;

        if (viewport_ubo)
        {
            viewport_ubo->Data()->Set(w, h);
            viewport_ubo->MarkDirty();
        }
    }

    void RenderDescriptorBindingSystem::RegisterPipelineMaterial(graph::ShaderProgram *material)
    {
        if (material)
            pipeline_materials.insert(material);
    }

    void RenderDescriptorBindingSystem::UnregisterPipelineMaterial(graph::ShaderProgram *material)
    {
        if (material)
            pipeline_materials.erase(material);
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
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

    uint32_t RenderDescriptorBindingSystem::RegisterTextureResource(const std::string &resource_id,
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

    uint32_t RenderDescriptorBindingSystem::GetBindlessHandle(const AnsiString &resource_id) const
    {
        if (resource_id.IsEmpty())
            return 0;

        const uint32_t *handle = materialization_resource_handles.GetValuePointer(resource_id);
        return handle ? *handle : 0;
    }

    void RenderDescriptorBindingSystem::Update(float /*deltaTime*/)
    {
        SyncBindingsForCurrentCommand(nullptr, true);
    }

    void RenderDescriptorBindingSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        // Critical for RenderDrawOnly path: Update() is not called there.
        SyncBindingsForCurrentCommand(cmd, false);
    }

    void RenderDescriptorBindingSystem::SyncBindingsForCurrentCommand(graph::RenderCmdBuffer *cmd, bool run_contract_diagnostics)
    {
        if (!context)
            return;

        if (run_contract_diagnostics)
            ValidateResourceLayoutsSideChannel();

        EnsureViewportUBO();

        ApplyResourceLayoutBindings(cmd);

    }

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveViewportUBO() const
    {
        return viewport_ubo ? viewport_ubo->GetGPUBuffer() : nullptr;
    }

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveCameraUBO() const
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

    const graph::IGPUBuffer *RenderDescriptorBindingSystem::ResolveSkyUBO()
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

    void RenderDescriptorBindingSystem::ApplyResourceLayoutBindings(graph::RenderCmdBuffer *cmd)
    {
        if (!context)
            return;

        const auto *viewport_ubo = ResolveViewportUBO();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();

        // P1: 全局 Scene UBO 描述符集 —— 一帧写一次（camera=0/sky=1/viewport=2/palette=3）。
        // camera/viewport 为所有材质必需；sky 与 color_palette 为可选（布局已带
        // PARTIALLY_BOUND 位，未静态使用的 binding 允许为空）。palette 由
        // LineRenderPipeline 等在初始化时写入 binding=3。
        auto *global_scene_set = GetGlobalSceneUBOSet(context);
        bool global_scene_valid = false;
        if (global_scene_set && global_scene_set->IsValid()
         && viewport_ubo && camera_ubo)
        {
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingCamera),   camera_ubo);
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingViewport), viewport_ubo);
            if (sky_ubo)
                global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingSky), sky_ubo);
            global_scene_valid = true;
        }
        else if (global_scene_set && global_scene_set->IsValid())
        {
            GLogWarning("[RDBinding] Scene UBO set not bound: camera=%p viewport=%p sky=%p",
                        (const void *)camera_ubo,
                        (const void *)viewport_ubo,
                        (const void *)sky_ubo);
        }

        const auto &cache = context->GetRenderFrameCache();

        std::unordered_set<const graph::ShaderProgram *> active_materials;

        auto log_bind_failure = [&](graph::ShaderProgram *material,
                                    MaterialBatch *batch,
                                    const graph::mtl::ShaderResourceSlot &req,
                                    const char *reason)
        {
            if (!material || req.name.empty())
                return;

            if (req.required)
            {
                if (batch)
                    batch->descriptor_bind_valid = false;

                GLogError("[DescriptorBinding] Bind failed: material=%s semantic=%s descriptor=%s reason=%s",
                          material->GetName().c_str(),
                          graph::mtl::GetDescriptorSemanticName(req.semantic),
                          req.name.c_str(),
                          reason ? reason : "unknown");
            }
            else
            {
                GLogWarning("[DescriptorBinding] Optional bind failed: material=%s semantic=%s descriptor=%s reason=%s",
                            material->GetName().c_str(),
                            graph::mtl::GetDescriptorSemanticName(req.semantic),
                            req.name.c_str(),
                            reason ? reason : "unknown");
            }
        };
        auto ensure_batch_mp = [&](graph::ShaderProgram *material,
                                   MaterialBatch *batch,
                                   const graph::DescriptorSetType set_type) -> graph::MaterialParameters *
        {
            if (!material || !batch)
                return nullptr;

            const size_t set_index = size_t(set_type);
            if (set_index >= graph::DESCRIPTOR_SET_TYPE_COUNT)
                return nullptr;

            if (batch->batch_descriptor_mp[set_index])
            {
                batch->has_batch_descriptor_overrides = true;
                return batch->batch_descriptor_mp[set_index];
            }

            if (!batch->device)
                return nullptr;

            const auto *desc_manager = material->GetDescriptorManager();
            const auto *pipeline_layout_data = material->GetPipelineLayoutData();
            if (!desc_manager || !pipeline_layout_data)
                return nullptr;

            // bindingCount=0 的集合（材质契约不含该集任何绑定）不创建 MP：
            // 空 layout 分配出的 DS 无可写 binding，任何 Update 都是 spec 违规
            //（VUID-10009）且会破坏堆。
            if (desc_manager->GetBindCount(set_type) == 0)
                return nullptr;

            auto *mp = batch->device->CreateMP(desc_manager, pipeline_layout_data, set_type);
            if (!mp)
                return nullptr;

            batch->batch_descriptor_mp[set_index] = mp;
            batch->has_batch_descriptor_overrides = true;
            return mp;
        };

        auto bind_ubo = [&](graph::ShaderProgram *material,
                            MaterialBatch *batch,
                            const graph::mtl::ShaderResourceSlot &req,
                            const graph::IGPUBuffer *gpu) -> bool
        {
            if (!material || !gpu)
                return false;

            bool ok = false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    ok = mp->BindUBO(req.name.c_str(), gpu, false);
            }
            else
            {
                ok = material->BindUBO(req.set_type, req.name.c_str(), gpu, false);
            }

            return ok;
        };

        auto apply_requirement = [&](graph::ShaderProgram *material,
                                     MaterialBatch *batch,
                                     const graph::mtl::ShaderResourceSlot &req)
        {
            switch (req.semantic)
            {
            case graph::mtl::DescriptorSemantic::ViewportInfo:
            {
                // P1: Scene UBO 已全局化，写入全局集（一帧一次），不再走 per-material bind。
                if (global_scene_valid)
                    break;

                if (viewport_ubo)
                {
                    if (!bind_ubo(material, batch, req, viewport_ubo))
                        log_bind_failure(material, batch, req, "bind viewport UBO failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::CameraInfo:
            {
                if (global_scene_valid)
                    break;

                if (camera_ubo)
                {
                    if (!bind_ubo(material, batch, req, camera_ubo))
                        log_bind_failure(material, batch, req, "bind camera UBO failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::SkyInfo:
            {
                if (global_scene_valid)
                    break;

                if (sky_ubo)
                {
                    if (!bind_ubo(material, batch, req, sky_ubo))
                        log_bind_failure(material, batch, req, "bind sky UBO failed");
                }
                break;
            }
            // LocalToWorld/LocalToWorldIndex/MeshDrawParams/MaterialPrivateData* 等行表
            // 语义已 BDA 化（A3-2/A5）：GLSL 无对应描述符声明 → 契约无相关 req ——
            // 绑定解析 case 随 A5-3 删除（数据经 pc_root + buffer_reference 寻址）。
            default:
                break;
            }
        };

        for (const auto &pair : cache.materialBatches)
        {
            graph::ShaderProgram *shader_program = pair.first.shader_program;
            if (!shader_program)
                continue;

            MaterialBatch *batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            active_materials.insert(shader_program);
            batch->descriptor_bind_valid = true;

            const auto &contract = shader_program->GetShaderResourceSchema();

            for (const auto &req : contract.resources)
            {
                if (req.name.empty())
                    continue;
                apply_requirement(shader_program, batch, req);
            }

        }

        // Bind scene-level UBOs to pipeline-registered materials (Line, Terrain, etc.)
        // These materials bypass the normal materialBatches path.
        for (graph::ShaderProgram *shader_program : pipeline_materials)
        {
            if (!shader_program)
                continue;

            active_materials.insert(shader_program);

            const auto &contract = shader_program->GetShaderResourceSchema();

            for (const auto &req : contract.resources)
            {
                if (req.name.empty())
                    continue;
                apply_requirement(shader_program, nullptr, req);
            }
        }

        // Set 0（Scene UBO）/ Set 3（Bindless 纹理）的 cmd buffer 绑定已移入
        // PipelineMaterialRenderer::Render（BindPipeline 之后按材质自身 layout 绑定）。
        // VVL 的 set 兼容 ID 取 layout 在 set 0..N 的全部 DSL 前缀，绑定 layout 必须与
        // draw 时管线 layout 一致，不能在 RDBS 用任意材质的 layout 统一绑定（08600）。

        for (auto it = resource_layout_last_ok.begin(); it != resource_layout_last_ok.end();)
        {
            if (active_materials.find(it->first) == active_materials.end())
                it = resource_layout_last_ok.erase(it);
            else
                ++it;
        }
    }

    bool RenderDescriptorBindingSystem::IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const
    {
        if (!context)
            return false;

        switch (semantic)
        {
        case graph::mtl::DescriptorSemantic::ViewportInfo:
            return viewport_ubo != nullptr;

        case graph::mtl::DescriptorSemantic::CameraInfo:
        {
            auto camera_system = context->GetSystem<CameraSystem>();
            return camera_system && camera_system->GetCameraUBO();
        }

        case graph::mtl::DescriptorSemantic::SkyInfo:
        {
            // sky 数据由 EnvironmentManager 统一提供（default 保证存在）
            graph::GraphicsContext *gc = nullptr;
            if (auto *rc = context->GetRenderContext())
                gc = rc->GetGraphicsContext();
            if (!gc)
                gc = context->GetGraphicsContext();
            auto *env_manager = gc ? gc->GetEnvironmentManager() : nullptr;
            return env_manager && env_manager->GetSkyUBO(graph::kEnvProfileDefault);
        }
        // 行表/材质数据/纹理语义已 BDA 化（A3/A5）：数据经 pc_root +
        // buffer_reference 寻址，RDBS 不再绑任何 SSBO。此处保留恒 true 仅为
        // 诊断静默——契约若含历史 req（A6 前过渡期），不误报 unresolved required。
        case graph::mtl::DescriptorSemantic::LocalToWorld:
        case graph::mtl::DescriptorSemantic::LocalToWorldIndex:
        case graph::mtl::DescriptorSemantic::MaterialColorPalette:
        case graph::mtl::DescriptorSemantic::MaterialPrivateData:
        case graph::mtl::DescriptorSemantic::MaterialTexture:
        case graph::mtl::DescriptorSemantic::MaterialSampler:
        case graph::mtl::DescriptorSemantic::MaterialPrivateDataIndex:
        case graph::mtl::DescriptorSemantic::MeshDrawParams:
            return true;
        case graph::mtl::DescriptorSemantic::Unknown:
        default:
            return false;
        }
    }

    void RenderDescriptorBindingSystem::ValidateResourceLayoutsSideChannel()
    {
        if (!resource_layout_diagnostics_enabled || !context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        ResourceLayoutDiagStats frame_stats;

        for (const auto &pair : cache.materialBatches)
        {
            const auto &key = pair.first;
            const graph::ShaderProgram *shader_program = key.shader_program;
            if (!shader_program)
                continue;

            ++frame_stats.materials_checked;

            const auto &contract = shader_program->GetShaderResourceSchema();

            bool all_required_ok = true;
            std::string first_error;

            for (const auto &req : contract.resources)
            {
                const bool resolvable = IsSemanticResolvable(req.semantic);
                if (resolvable)
                    continue;

                if (req.required && !req.allow_fallback)
                {
                    ++frame_stats.required_missing;
                    all_required_ok = false;

                    if (first_error.empty())
                    {
                        first_error = "missing semantic=";
                        first_error += graph::mtl::GetDescriptorSemanticName(req.semantic);
                        first_error += " name=";
                        first_error += req.name.empty() ? "<empty>" : req.name;
                    }
                }
                else
                {
                    ++frame_stats.optional_missing;

                    if (req.allow_fallback)
                        ++frame_stats.fallback_hits;
                }
            }

            auto it = resource_layout_last_ok.find(shader_program);
            if (it == resource_layout_last_ok.end())
            {
                resource_layout_last_ok.emplace(shader_program, all_required_ok);

                if (!all_required_ok)
                    ++frame_stats.materials_unresolved;

                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s unresolved required contract: %s",
                               shader_program->GetName().c_str(),
                               first_error.c_str());
                }
                continue;
            }

            if (it->second != all_required_ok)
            {
                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s contract changed to unresolved: %s",
                               shader_program->GetName().c_str(),
                               first_error.c_str());
                }
                else
                {
                    LogInfo("[DescriptorContract] material=%s contract resolved", shader_program->GetName().c_str());
                }

                it->second = all_required_ok;
            }

            if (!all_required_ok)
                ++frame_stats.materials_unresolved;
        }

        if (frame_stats != last_contract_stats)
        {
            LogInfo("[DescriptorContract] frame stats: checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u",
                    frame_stats.materials_checked,
                    frame_stats.materials_unresolved,
                    frame_stats.required_missing,
                    frame_stats.optional_missing,
                    frame_stats.fallback_hits);

            last_contract_stats = frame_stats;
        }
    }
}
