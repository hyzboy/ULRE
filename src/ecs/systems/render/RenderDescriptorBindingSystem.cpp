#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
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
#include<hgl/graph/module/ResourceDomainManager.h>
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
        std::string BuildTextureResourceId(const graph::Texture *texture)
        {
            if (!texture)
                return {};

            return "texid:" + std::to_string(texture->GetID());
        }

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

        graph::ResourceDomainManager *GetResourceDomainManager(hgl::ecs::ECSContext *ctx)
        {
            if (!ctx)
                return nullptr;

            if (auto *rc = ctx->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetResourceDomainManager();
            }

            if (auto *gc = ctx->GetGraphicsContext())
                return gc->GetResourceDomainManager();

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
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameBusinessSyncSystem>();
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

        if (auto *domain_manager = GetResourceDomainManager(context))
            domain_manager->Touch(graph::mtl::SSBOAddress{ssbo_type, ssbo_id, 0});

        return true;
    }

    uint32_t RenderDescriptorBindingSystem::RegisterTexture2DResource(const std::string &resource_id,
                                                                      graph::Texture *tex,
                                                                      graph::Sampler *sampler,
                                                                      graph::BindlessTextureManager *bindless_mgr)
    {
        if (!tex || !sampler || !bindless_mgr)
            return 0;

        std::string rid = resource_id;
        if (rid.empty())
            rid = BuildTextureResourceId(tex);

        if (rid.empty())
            return 0;

        const uint32_t handle = bindless_mgr->Register2D(tex, sampler);
        if (handle != 0)
            materialization_resource_handles.Add(AnsiString(rid.c_str()), handle);

        return handle;
    }

    uint32_t RenderDescriptorBindingSystem::RegisterTexture2DArrayResource(const std::string &resource_id,
                                                                             graph::Texture *tex,
                                                                             graph::Sampler *sampler,
                                                                             graph::BindlessTextureManager *bindless_mgr)
    {
        if (!tex || !sampler || !bindless_mgr)
            return 0;

        std::string rid = resource_id;
        if (rid.empty())
            rid = BuildTextureResourceId(tex);

        if (rid.empty())
            return 0;

        const uint32_t handle = bindless_mgr->Register2DArray(tex, sampler);
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

        auto environment_system = context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
        {
            environment_system = context->RegisterRenderSystem<EnvironmentSystem>();
            if (environment_system && context->IsActive())
            {
                environment_system->OnDependenciesReady();
                environment_system->Initialize();
            }
        }

        if (!environment_system)
            return nullptr;

        environment_system->EditSkyInfo();

        auto *sky_ubo = environment_system->GetSkyUBO();
        if (!sky_ubo)
            return nullptr;

        return sky_ubo->GetGPUBuffer();
    }

    void RenderDescriptorBindingSystem::ApplyResourceLayoutBindings(graph::RenderCmdBuffer *cmd)
    {
        if (!context)
            return;

        const auto *viewport_ubo = ResolveViewportUBO();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();
        auto *domain_manager = GetResourceDomainManager(context);

        // P1: 全局 Scene UBO 描述符集 —— 一帧写一次（camera=0/sky=1/viewport=2）。
        // 3 个 UBO 全部有效时全局集才可绑定（UBO 无 PARTIALLY_BOUND 位）。
        auto *global_scene_set = GetGlobalSceneUBOSet(context);
        bool global_scene_valid = false;
        if (global_scene_set && global_scene_set->IsValid()
         && viewport_ubo && camera_ubo && sky_ubo)
        {
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingCamera),   camera_ubo);
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingSky),      sky_ubo);
            global_scene_set->UpdateUBO(uint32_t(graph::kSceneBindingViewport), viewport_ubo);
            global_scene_valid = true;
        }

        auto resolve_domain_ssbo = [&](const graph::mtl::SSBOAddress &address, const char *semantic_tag) -> const graph::IGPUBuffer *
        {
            if (!domain_manager)
                return nullptr;

            graph::ResourceDomainBinding binding{};
            if (!domain_manager->TryGetBinding(address, binding) || !binding.buffer)
                return nullptr;

            const uint32_t expected_version = graph::mtl::GetSSBOTypeStructVersion(address.ssbo_type);
            const uint32_t expected_stride = graph::mtl::GetSSBOTypeStructStride(address.ssbo_type);
            if (expected_version > 0 && expected_stride > 0)
            {
                if (binding.element_stride != 0 && binding.element_stride != expected_stride)
                {
                    GLogError("[R11] Skip binding %s: type=%s ssbo_id=%u version=%u expected_stride=%u actual_stride=%u",
                              semantic_tag ? semantic_tag : "UnknownSemantic",
                              graph::mtl::GetSSBOTypeName(address.ssbo_type),
                              address.ssbo_id,
                              expected_version,
                              expected_stride,
                              binding.element_stride);
                    return nullptr;
                }
            }

            return binding.buffer->GetGPUBuffer();
        };

        auto validate_runtime_ssbo_stride = [&](const char *semantic_tag,
                                                const graph::mtl::SSBOType ssbo_type,
                                                const graph::IGPUBuffer *buffer,
                                                const uint32_t element_count,
                                                const uint32_t expected_stride_override = 0) -> bool
        {
            if (!buffer)
                return false;

            const uint32_t expected_version = graph::mtl::GetSSBOTypeStructVersion(ssbo_type);
            const uint32_t expected_stride = expected_stride_override > 0
                                           ? expected_stride_override
                                           : graph::mtl::GetSSBOTypeStructStride(ssbo_type);
            if (expected_version == 0 || expected_stride == 0)
                return true;

            if (element_count == 0)
                return false;

            const VkDeviceSize byte_size = buffer->GetSize();
            if (byte_size == 0)
            {
                GLogError("[R11] Skip binding %s: type=%s version=%u invalid byte_size=0",
                          semantic_tag ? semantic_tag : "UnknownSemantic",
                          graph::mtl::GetSSBOTypeName(ssbo_type),
                          expected_version);
                return false;
            }

            const VkDeviceSize min_required = static_cast<VkDeviceSize>(expected_stride) * static_cast<VkDeviceSize>(element_count);
            if (byte_size < min_required)
            {
                GLogError("[R11] Skip binding %s: type=%s version=%u expected_stride=%u element_count=%u min_required=%llu actual_size=%llu",
                          semantic_tag ? semantic_tag : "UnknownSemantic",
                          graph::mtl::GetSSBOTypeName(ssbo_type),
                          expected_version,
                          expected_stride,
                          element_count,
                          static_cast<unsigned long long>(min_required),
                          static_cast<unsigned long long>(byte_size));
                return false;
            }

            if ((byte_size % expected_stride) != 0)
            {
                GLogError("[R11] Skip binding %s: type=%s version=%u expected_stride=%u invalid_byte_size=%llu",
                          semantic_tag ? semantic_tag : "UnknownSemantic",
                          graph::mtl::GetSSBOTypeName(ssbo_type),
                          expected_version,
                          expected_stride,
                          static_cast<unsigned long long>(byte_size));
                return false;
            }

            return true;
        };

        const auto &cache = context->GetRenderFrameCache();

        std::unordered_set<const graph::ShaderProgram *> active_materials;

        std::unordered_set<std::string> missing_ssbo_warned_keys;

        auto log_missing_ssbo_once = [&](graph::ShaderProgram *material,
                                         const graph::mtl::ShaderResourceSlot &req,
                                         const char *reason,
                                         int32_t slot = -1)
        {
            if (!material || req.name.empty())
                return;

            std::string key = material->GetName().c_str();
            key += '|';
            key += req.name;
            key += '|';
            key += std::to_string(static_cast<uint32_t>(req.semantic));
            key += '|';
            key += std::to_string(static_cast<uint32_t>(req.ssbo_type));
            key += '|';
            key += std::to_string(req.ssbo_id);
            key += '|';
            key += std::to_string(slot);
            key += '|';
            key += reason ? reason : "unknown";

            if (!missing_ssbo_warned_keys.insert(std::move(key)).second)
                return;

            if (req.required)
            {
                GLogError("[DescriptorBinding] Missing SSBO binding: material=%s semantic=%s descriptor=%s type=%s ssbo_id=%u slot=%d reason=%s. Resource producer must register it via RegisterMaterialStructLayout(...) and ResourceDomainManager::RegisterBuffer(...).",
                          material->GetName().c_str(),
                          graph::mtl::GetDescriptorSemanticName(req.semantic),
                          req.name.c_str(),
                          graph::mtl::GetSSBOTypeName(req.ssbo_type),
                          req.ssbo_id,
                          slot,
                          reason ? reason : "unknown");
            }
            else
            {
                GLogWarning("[DescriptorBinding] Missing SSBO binding: material=%s semantic=%s descriptor=%s type=%s ssbo_id=%u slot=%d reason=%s. Resource producer must register it via RegisterMaterialStructLayout(...) and ResourceDomainManager::RegisterBuffer(...).",
                            material->GetName().c_str(),
                            graph::mtl::GetDescriptorSemanticName(req.semantic),
                            req.name.c_str(),
                            graph::mtl::GetSSBOTypeName(req.ssbo_type),
                            req.ssbo_id,
                            slot,
                            reason ? reason : "unknown");
            }
        };
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

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindUBO(req.name.c_str(), gpu, false);
                return false;
            }

            return material->BindUBO(req.set_type, req.name.c_str(), gpu, false);
        };

        auto bind_ssbo = [&](graph::ShaderProgram *material,
                             MaterialBatch *batch,
                             const graph::mtl::ShaderResourceSlot &req,
                             const graph::IGPUBuffer *gpu) -> bool
        {
            if (!material || !gpu)
                return false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindSSBO(req.name.c_str(), gpu, false);
                return false;
            }

            return material->BindSSBO(req.set_type, req.name.c_str(), gpu, false);
        };

        auto resolve_recipe_batch_struct_ssbo_id = [&](graph::ShaderProgram *material,
                                                       MaterialBatch *batch,
                                                       const graph::mtl::ShaderResourceSlot &req,
                                                       uint32_t &out_ssbo_id) -> bool
        {
            if (!batch)
                return false;

            bool found = false;
            uint32_t ssbo_id = 0;

            for (RenderItem *item : batch->items)
            {
                auto *primitive_item = dynamic_cast<PrimitiveRenderItem *>(item);
                if (!primitive_item)
                    continue;

                auto primitive_comp = primitive_item->GetPrimitiveComponent();
                if (!primitive_comp)
                    continue;

                uint32_t candidate_ssbo_id = 0;
                bool has_candidate = false;

                if (auto *entity = primitive_item->GetEntity())
                {
                    auto material_comp = entity->GetComponent<MaterialComponent>();
                    if (const auto *resolved = material_comp
                        ? material_comp->FindResolvedSSBOBinding(
                            req.name.c_str(), req.data_slot, req.ssbo_type)
                        : nullptr)
                    {
                        candidate_ssbo_id = resolved->ssbo_id;
                        has_candidate = true;
                    }
                }

                if (!has_candidate)
                {
                    graph::mtl::MaterialRecipe effective_recipe{};
                    if (primitive_comp->BuildResolvedAuthoringMaterialRecipe(effective_recipe, material))
                    {
                        if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBinding(
                                effective_recipe, req.name.c_str(), req.data_slot, req.ssbo_type))
                        {
                            candidate_ssbo_id = asset->ssbo_id;
                            has_candidate = true;
                        }

                        if (has_candidate)
                        {
                            if (auto *entity = primitive_item->GetEntity())
                            {
                                if (auto material_comp = entity->GetComponent<MaterialComponent>())
                                    material_comp->SetResolvedSSBOBinding(
                                        req.name.c_str(),
                                        req.data_slot,
                                        req.ssbo_type,
                                        candidate_ssbo_id);
                            }
                        }
                    }
                }

                if (!has_candidate)
                    return false;

                if (!found)
                {
                    ssbo_id = candidate_ssbo_id;
                    found = true;
                    continue;
                }

                if (ssbo_id != candidate_ssbo_id)
                {
                    GLogError("[DescriptorBinding] Recipe batch struct mismatch: material=%s semantic=%s descriptor=%s slot=%u expected_ssbo_id=%u actual_ssbo_id=%u.",
                              material->GetName().c_str(),
                              graph::mtl::GetDescriptorSemanticName(req.semantic),
                              req.name.c_str(),
                              req.data_slot,
                              ssbo_id,
                              candidate_ssbo_id);
                    batch->descriptor_bind_valid = false;
                    return false;
                }
            }

            if (!found)
                return false;

            out_ssbo_id = ssbo_id;
            return true;
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
            case graph::mtl::DescriptorSemantic::LocalToWorld:
            {
                if (batch
                 && batch->transform_buffer
                 && material->hasLocalToWorld())
                {
                    auto *transform_data_buffer = batch->transform_buffer->GetTransformDataBuffer();
                    const auto *transform_gpu_buffer = transform_data_buffer ? transform_data_buffer->GetGPUBuffer() : nullptr;

                    if (transform_gpu_buffer)
                    {
                        if (!bind_ssbo(material, batch, req, transform_gpu_buffer))
                            log_bind_failure(material, batch, req, "bind LocalToWorld SSBO failed");
                        break;
                    }
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::LocalToWorldIndexTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;

                // Prefer the per-batch buffer written in draw order by PrimitiveBatchPipeline.
                if (batch && batch->l2w_index_rows_buffer)
                {
                    const auto *candidate = batch->l2w_index_rows_buffer->GetGPUBuffer();
                    const uint32_t element_count = static_cast<uint32_t>(batch->items.size());
                    if (validate_runtime_ssbo_stride("LocalToWorldIndexTable",
                                                     graph::mtl::SSBOType::TransformIndexRows,
                                                     candidate,
                                                     element_count,
                                                     sizeof(uint32_t)))
                    {
                        table_buffer = candidate;
                    }
                }

                // Fallback: global TransformIndexRows from TransformAssignmentBuffer.
                if (!table_buffer && batch && batch->transform_buffer)
                {
                    auto *rows_buffer = batch->transform_buffer->GetTransformIndexRowsBuffer();
                    const auto *candidate = rows_buffer ? rows_buffer->GetGPUBuffer() : nullptr;
                    const uint32_t element_count = static_cast<uint32_t>(batch->items.size());
                    if (validate_runtime_ssbo_stride("LocalToWorldIndexTable",
                                                     graph::mtl::SSBOType::TransformIndexRows,
                                                     candidate,
                                                     element_count,
                                                     sizeof(uint32_t)))
                    {
                        table_buffer = candidate;
                    }
                }

                                // Pipeline-only materials (no MaterialBatch) still need l2w_index_rows.
                // Resolve from TransformSystem directly before falling back to domain cache.
                if (!table_buffer && context)
                {
                    auto transform_system = context->GetSystem<TransformSystem>();
                    if (transform_system)
                    {
                        transform_system->EnsureTransformBuffer();
                        auto *transform_buffer = transform_system->GetTransformBuffer();
                        if (transform_buffer)
                        {
                            auto *rows_buffer = transform_buffer->GetTransformIndexRowsBuffer();
                            table_buffer = rows_buffer ? rows_buffer->GetGPUBuffer() : nullptr;
                        }
                    }
                }

                if (!table_buffer && domain_manager)
                {
                    table_buffer = resolve_domain_ssbo(
                        graph::mtl::SSBOAddress{
                            graph::mtl::SSBOType::TransformIndexRows,
                            graph::mtl::ECSReservedSSBOId::TransformIndexRows,
                            0},
                        "LocalToWorldIndexTable");
                }

                if (table_buffer)
                {
                    if (!bind_ssbo(material, batch, req, table_buffer))
                        log_bind_failure(material, batch, req, "bind LocalToWorldIndexTable failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialDataSlotData:
            {
                uint32_t resolved_ssbo_id = req.ssbo_id;
                if (batch)
                {
                    if (!resolve_recipe_batch_struct_ssbo_id(material, batch, req, resolved_ssbo_id))
                    {
                        log_bind_failure(material, batch, req, "unresolved MaterialDataSlotData binding");
                        break;
                    }
                }

                const graph::IGPUBuffer *material_data_ssbo = resolve_domain_ssbo(
                    graph::mtl::SSBOAddress{req.ssbo_type, resolved_ssbo_id, 0},
                    "MaterialDataSlot");

                if (material_data_ssbo)
                {
                    if (!bind_ssbo(material, batch, req, material_data_ssbo))
                        log_bind_failure(material, batch, req, "bind MaterialDataSlot failed");
                }
                else
                {
                    log_missing_ssbo_once(material, req, "domain binding not found", 0);
                    if (batch && req.required)
                        batch->descriptor_bind_valid = false;
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;

                // Prefer per-batch texture layer rows SSBO (keyed by the
                // primitive's own data_index VALUE, written in draw order by
                // PrimitiveBatchPipeline).
                if (batch && batch->texture_layer_rows_buffer)
                    table_buffer = batch->texture_layer_rows_buffer->GetGPUBuffer();

                // Fall back to domain SSBO.
                if (!table_buffer)
                {
                    table_buffer = resolve_domain_ssbo(
                        graph::mtl::SSBOAddress{
                            req.ssbo_type,
                            req.ssbo_id,
                            static_cast<uint32_t>(req.texture_slot)},
                        "MaterialTextureLayerTable");
                }

                if (table_buffer)
                {
                    if (!bind_ssbo(material, batch, req, table_buffer))
                        log_bind_failure(material, batch, req, "bind MaterialTextureLayerTable failed");
                }
                else
                {
                    log_missing_ssbo_once(material, req, batch ? "batch rows missing and domain binding not found" : "domain binding not found", static_cast<int32_t>(req.texture_slot));
                    if (batch && req.required)
                        batch->descriptor_bind_valid = false;
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;

                // Prefer per-batch DataIndex rows SSBO (written in draw order by PrimitiveBatchPipeline).
                if (batch && batch->material_data_index_rows_buffer)
                    table_buffer = batch->material_data_index_rows_buffer->GetGPUBuffer();

                // Fall back to domain SSBO.
                if (!table_buffer)
                {
                    table_buffer = resolve_domain_ssbo(
                        graph::mtl::SSBOAddress{
                            req.ssbo_type,
                            req.ssbo_id,
                            req.data_slot},
                        "MaterialDataIndexTable");
                }

                if (table_buffer)
                {
                    if (!bind_ssbo(material, batch, req, table_buffer))
                        log_bind_failure(material, batch, req, "bind MaterialDataIndexTable failed");
                }
                else
                {
                    log_missing_ssbo_once(material, req, batch ? "batch rows missing and domain binding not found" : "domain binding not found", static_cast<int32_t>(req.data_slot));
                    if (batch && req.required)
                        batch->descriptor_bind_valid = false;
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialColorPalette:
            {
                // MaterialColorPalette is explicitly declared and contract-validated.
                // The current owner-bound path is non-ECS (for example LineRenderPipeline
                // binds SBS_ColorPalette directly), so RDBS intentionally does not inject it.
                break;
            }
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

        // 全局描述符集（P1/P2）：Set 0 Scene / Set 3 Bindless 一帧绑一次。
        // 所有材质的 pipeline layout 在 Set 0/3 都使用同一个全局 layout
        //（scene_layout_ / bindless_layout_），所以取任一活跃材质的布局绑定一次即可，
        // 后续不同材质换管线时该绑定仍保持有效（set layout 兼容）。
        if (auto *render_context = context->GetRenderContext())
        {
            auto *current_cmd = cmd ? cmd : render_context->GetCurrentRenderCmdBuffer();

            if (current_cmd)
            {
                auto *bindless_mgr = render_context->GetManager<graph::BindlessTextureManager>();

                VkPipelineLayout bind_layout = VK_NULL_HANDLE;

                for (const graph::ShaderProgram *material : active_materials)
                {
                    if (!material)
                        continue;

                    bind_layout = material->GetPipelineLayout();
                    if (bind_layout != VK_NULL_HANDLE)
                        break;
                }

                if (bind_layout != VK_NULL_HANDLE)
                {
                    if (global_scene_valid && global_scene_set)
                        global_scene_set->BindToCmd(*current_cmd, bind_layout);

                    if (bindless_mgr && bindless_mgr->IsValid())
                        bindless_mgr->BindToCmd(*current_cmd,
                                                bind_layout,
                                                static_cast<uint32_t>(graph::DescriptorSetType::Bindless));
                }
            }
        }

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
            auto environment_system = context->GetSystem<EnvironmentSystem>();
            return environment_system && environment_system->GetSkyUBO();
        }
        case graph::mtl::DescriptorSemantic::LocalToWorld:
        case graph::mtl::DescriptorSemantic::LocalToWorldIndexTable:
        case graph::mtl::DescriptorSemantic::MaterialColorPalette:
        case graph::mtl::DescriptorSemantic::MaterialDataSlotData:
        case graph::mtl::DescriptorSemantic::MaterialTexture:
        case graph::mtl::DescriptorSemantic::MaterialSampler:
            return true;
        case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
            return true;
        case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
            return true;
        case graph::mtl::DescriptorSemantic::Unknown:
        case graph::mtl::DescriptorSemantic::Custom:
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
