#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<unordered_set>
#include<cstdlib>
#include<cstdio>
#include<string>
#include<vector>

namespace hgl::ecs
{
    namespace
    {
        enum class DomainDirectMITBindResult : uint8_t
        {
            Success = 0,
            InvalidInput,
            NotDomainDirectMode,
            MaterialNoTextureArraySlots,
            NoResolvedSlotData,
            NoMITEntryWidth,
            ZeroTotalSize,
            EnsureBufferFailed,
            UploadFailed,
            MissingGPUBuffer,
            Count
        };

        const char *ToString(DomainDirectMITBindResult r)
        {
            switch (r)
            {
            case DomainDirectMITBindResult::Success: return "success";
            case DomainDirectMITBindResult::InvalidInput: return "invalid_input";
            case DomainDirectMITBindResult::NotDomainDirectMode: return "not_domain_direct_mode";
            case DomainDirectMITBindResult::MaterialNoTextureArraySlots: return "material_no_texture_array_slots";
            case DomainDirectMITBindResult::NoResolvedSlotData: return "no_resolved_slot_data";
            case DomainDirectMITBindResult::NoMITEntryWidth: return "no_mit_entry_width";
            case DomainDirectMITBindResult::ZeroTotalSize: return "zero_total_size";
            case DomainDirectMITBindResult::EnsureBufferFailed: return "ensure_buffer_failed";
            case DomainDirectMITBindResult::UploadFailed: return "upload_failed";
            case DomainDirectMITBindResult::MissingGPUBuffer: return "missing_gpu_buffer";
            default: return "unknown";
            }
        }

        void ApplySceneUBOBindings(graph::MaterialTemplate *material,
                                   const graph::mtl::DescriptorBindingSlots &contract,
                                   const std::array<graph::UBOAccessorBase *, graph::mtl::UBODescriptorSemanticCount> &scene_ubo_resolvers)
        {
            if (!material)
                return;

            for (size_t i = 1; i < graph::mtl::UBODescriptorSemanticCount; ++i)
            {
                if (contract.ubos[i] == 0)
                    continue;

                auto *accessor = scene_ubo_resolvers[i];
                if (accessor)
                    material->BindUBO(accessor);
            }
        }

        TextureBindingSlot ToBindingKey(const graph::mtl::SamplerSlot slot)
        {
            return static_cast<TextureBindingSlot>(slot);
        }

        bool TryResolveTextureSlot(const char *descriptor_name, graph::mtl::SamplerSlot &slot)
        {
            return graph::mtl::TryGetSlotFromDescriptorName(descriptor_name, slot);
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

        bool HasResolvedInstanceSlotForDomain(const hgl::ecs::RenderItem *item,
                                              const graph::MaterialResourceDomain *domain)
        {
            if (!item)
                return false;

            const auto& binding = item->GetEntityMaterialBinding();

            // Phase B: instance-indexed descriptor paths must rely on explicit
            // instance eligibility (domain + non-negative mi_id), independent
            // of draw-path resolved-slot validity.
            if (!binding.material_template)
                return false;

            if (binding.domain != domain)
                return false;

            return binding.mi_id >= 0;
        }

        bool TryBindDomainDirectMIData(const hgl::ecs::MaterialBatch *batch,
                                       graph::MaterialTemplate *material,
                                       graph::MaterialResourceDomain *domain,
                                       graph::BufferManager *buffer_manager)
        {
            if (!batch || !material || !domain || !buffer_manager)
                return false;

            if (!batch->mi_buffer || !batch->mi_buffer->IsUsingResolvedDomainMIID())
                return false;

            int max_mi_id = -1;
            for (auto *item : batch->items)
            {
                if (!HasResolvedInstanceSlotForDomain(item, domain))
                    continue;

                const auto& binding = item->GetEntityMaterialBinding();

                if (binding.mi_id > max_mi_id)
                    max_mi_id = binding.mi_id;
            }

            if (max_mi_id < 0)
                return false;

            const uint32_t needed = static_cast<uint32_t>(max_mi_id + 1);
            if (!domain->EnsureMIBuffer(buffer_manager, needed))
                return false;

            domain->MarkMIDirtyRange(0, needed);
            if (!domain->UploadMIDirtyRange())
                return false;

            auto *mi_buf = domain->GetMIGPUBuffer();
            auto *mi_gpu = mi_buf ? mi_buf->GetGPUBuffer() : nullptr;
            if (!mi_gpu)
                return false;

            material->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialInstanceData, mi_gpu);
            return true;
        }

        bool TryBindDomainDirectMITData(const hgl::ecs::MaterialBatch *batch,
                                        graph::MaterialTemplate *material,
                                        graph::MaterialResourceDomain *domain,
                                        graph::BufferManager *buffer_manager,
                                        DomainDirectMITBindResult *out_result = nullptr)
        {
            if (out_result)
                *out_result = DomainDirectMITBindResult::Success;

            if (!batch || !material || !domain || !buffer_manager)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::InvalidInput;
                return false;
            }

            if (!batch->mi_buffer || !batch->mi_buffer->IsUsingResolvedDomainMIID())
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::NotDomainDirectMode;
                return false;
            }

            if (material->GetTextureArraySlotFlags() == 0)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::MaterialNoTextureArraySlots;
                return false;
            }

            int max_mi_id = -1;
            uint32_t per_entry_uint_count = 0;

            for (auto *item : batch->items)
            {
                if (!HasResolvedInstanceSlotForDomain(item, domain))
                    continue;

                const auto& binding = item->GetEntityMaterialBinding();

                if (binding.mi_id > max_mi_id)
                    max_mi_id = binding.mi_id;

                if (per_entry_uint_count == 0 && binding.mit_count > 0)
                    per_entry_uint_count = binding.mit_count;
            }

            if (max_mi_id < 0)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::NoResolvedSlotData;
                return false;
            }

            if (per_entry_uint_count == 0)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::NoMITEntryWidth;
                return false;
            }

            const uint32_t slot_count = static_cast<uint32_t>(max_mi_id + 1);
            const uint32_t total_uint_count = slot_count * per_entry_uint_count;

            if (total_uint_count == 0)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::ZeroTotalSize;
                return false;
            }

            std::vector<uint32_t> mit_staging(total_uint_count, 0u);
            for (auto *item : batch->items)
            {
                if (!HasResolvedInstanceSlotForDomain(item, domain))
                    continue;

                const auto& binding = item->GetEntityMaterialBinding();

                if (!binding.mit_data || binding.mit_count == 0)
                    continue;

                const uint32_t copy_count = std::min(per_entry_uint_count, binding.mit_count);
                uint32_t *dst = mit_staging.data() + static_cast<uint32_t>(binding.mi_id) * per_entry_uint_count;
                memcpy(dst, binding.mit_data, static_cast<size_t>(copy_count) * sizeof(uint32_t));
            }

            if (!domain->EnsureMITBuffer(buffer_manager, total_uint_count))
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::EnsureBufferFailed;
                return false;
            }

            domain->MarkMITDirtyRange(0, total_uint_count);
            if (!domain->UploadMITDirtyRange(mit_staging.data(), total_uint_count))
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::UploadFailed;
                return false;
            }

            auto *mit_buf = domain->GetMITGPUBuffer();
            auto *mit_gpu = mit_buf ? mit_buf->GetGPUBuffer() : nullptr;
            if (!mit_gpu)
            {
                if (out_result)
                    *out_result = DomainDirectMITBindResult::MissingGPUBuffer;
                return false;
            }

            material->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID, mit_gpu);
            return true;
        }

        void EnsureSceneUBOSourceSystems(hgl::ecs::ECSContext *context,
                                         hgl::ecs::CameraSystem *&camera_system,
                                         hgl::ecs::EnvironmentSystem *&environment_system)
        {
            if (!context)
                return;

            if (!camera_system)
            {
                auto cs = context->GetSystem<hgl::ecs::CameraSystem>();
                camera_system = cs ? cs.get() : nullptr;
            }

            if (!environment_system)
            {
                auto es = context->GetSystem<hgl::ecs::EnvironmentSystem>();
                if (!es)
                {
                    es = context->RegisterRenderSystem<hgl::ecs::EnvironmentSystem>();
                    if (es && context->IsActive())
                    {
                        es->OnDependenciesReady();
                        es->Initialize();
                    }
                }
                environment_system = es ? es.get() : nullptr;
            }
        }

    } // anonymous namespace

    RenderDescriptorBindingSystem::RenderDescriptorBindingSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameBusinessSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();

        InitializeResolvers();
    }

    RenderDescriptorBindingSystem::~RenderDescriptorBindingSystem()
    {
        ReleaseViewportUBO();
    }

    void RenderDescriptorBindingSystem::InitializeResolvers()
    {
        scene_ubo_resolvers.fill(nullptr);
        RefreshSceneUBOResolvers();
    }

    void RenderDescriptorBindingSystem::RegisterSceneUBOResolver(graph::UBOAccessorBase *ubo_accessor)
    {
        if (!ubo_accessor)
            return;

        const auto semantic = ubo_accessor->GetSemantic();
        const size_t index = size_t(semantic);
        if (index >= scene_ubo_resolvers.size())
            return;

        scene_ubo_resolvers[index] = ubo_accessor;
    }

    void RenderDescriptorBindingSystem::UnregisterSceneUBOResolver(graph::mtl::UBODescriptorSemantic semantic)
    {
        const size_t index = size_t(semantic);
        if (index >= scene_ubo_resolvers.size())
            return;

        scene_ubo_resolvers[index] = nullptr;
    }

    void RenderDescriptorBindingSystem::RefreshSceneUBOResolvers()
    {
        EnsureSceneUBOSourceSystems(context, camera_system, environment_system);

        scene_ubo_resolvers[size_t(graph::mtl::UBODescriptorSemantic::ViewportInfo)] = viewport_ubo;

        scene_ubo_resolvers[size_t(graph::mtl::UBODescriptorSemantic::CameraInfo)] =
            camera_system ? camera_system->GetCameraUBO() : nullptr;

        if (environment_system)
            environment_system->EditSkyInfo();

        scene_ubo_resolvers[size_t(graph::mtl::UBODescriptorSemantic::SkyInfo)] =
            environment_system ? environment_system->GetSkyUBO() : nullptr;
    }

    void RenderDescriptorBindingSystem::EnsureViewportUBO()
    {
        if (viewport_ubo || !context)
            return;

        auto *bm = GetBufferManager(context);
        if (!bm)
            return;

        viewport_ubo = bm->CreateUBO<graph::UBOViewportInfo>();
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

        scene_ubo_resolvers[size_t(graph::mtl::UBODescriptorSemantic::ViewportInfo)] = viewport_ubo;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTexture(graph::MaterialTemplate *material,
                                                                graph::mtl::SamplerSlot slot,
                                                                graph::Texture *texture)
    {
        if (!material || !texture)
            return false;

        auto &binding = material_resource_bindings[material][ToBindingKey(slot)];
        binding.texture = texture;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTextureSampler(graph::MaterialTemplate *material,
                                                                       graph::mtl::SamplerSlot slot,
                                                                       graph::Texture *texture,
                                                                       graph::Sampler *sampler)
    {
        if (!RegisterMaterialTexture(material, slot, texture))
            return false;

        if (!sampler)
            return false;

        auto &binding = material_resource_bindings[material][ToBindingKey(slot)];
        binding.sampler = sampler;
        return true;
    }

    void RenderDescriptorBindingSystem::RemoveMaterialBinding(graph::MaterialTemplate *material, graph::mtl::SamplerSlot slot)
    {
        if (!material)
            return;

        auto material_it = material_resource_bindings.find(material);
        if (material_it == material_resource_bindings.end())
            return;

        material_it->second.erase(ToBindingKey(slot));
        if (material_it->second.empty())
            material_resource_bindings.erase(material_it);
    }

    void RenderDescriptorBindingSystem::ClearMaterialBindings(graph::MaterialTemplate *material)
    {
        if (!material)
            return;

        material_resource_bindings.erase(material);
    }

    void RenderDescriptorBindingSystem::RegisterPipelineMaterial(graph::MaterialTemplate *material)
    {
        if (material)
            pipeline_materials.insert(material);
    }

    void RenderDescriptorBindingSystem::UnregisterPipelineMaterial(graph::MaterialTemplate *material)
    {
        if (material)
            pipeline_materials.erase(material);
    }

    // Phase 2 — DomainMaterialBinding registration

    void RenderDescriptorBindingSystem::RegisterDomainBinding(graph::DomainMaterialBinding *binding)
    {
        if (binding)
            registered_domain_bindings.insert(binding);
    }

    void RenderDescriptorBindingSystem::UnregisterDomainBinding(graph::DomainMaterialBinding *binding)
    {
        if (!binding)
            return;
        registered_domain_bindings.erase(binding);
        domain_resource_bindings.erase(binding);
    }

    bool RenderDescriptorBindingSystem::RegisterDomainTexture(graph::DomainMaterialBinding *binding,
                                                              graph::mtl::SamplerSlot slot,
                                                              graph::Texture *tex)
    {
        if (!binding || !tex)
            return false;
        auto &binding_slot = domain_resource_bindings[binding][ToBindingKey(slot)];
        binding_slot.texture = tex;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterDomainTextureSampler(graph::DomainMaterialBinding *binding,
                                                                     graph::mtl::SamplerSlot slot,
                                                                     graph::Texture *tex,
                                                                     graph::Sampler *sampler)
    {
        if (!RegisterDomainTexture(binding, slot, tex))
            return false;
        if (!sampler)
            return false;
        auto &binding_slot = domain_resource_bindings[binding][ToBindingKey(slot)];
        binding_slot.sampler = sampler;
        return true;
    }

    void RenderDescriptorBindingSystem::ClearDomainBindings(graph::DomainMaterialBinding *binding)
    {
        if (binding)
            domain_resource_bindings.erase(binding);
    }

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindMaterialResourceBinding(const graph::MaterialTemplate *material,
                                                                                                                             graph::mtl::SamplerSlot slot) const
    {
        if (!material)
            return nullptr;

        auto material_it = material_resource_bindings.find(material);
        if (material_it == material_resource_bindings.end())
            return nullptr;

        auto resource_it = material_it->second.find(ToBindingKey(slot));
        if (resource_it == material_it->second.end())
            return nullptr;

        return &resource_it->second;
    }

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindDomainResourceBinding(
        const graph::DomainMaterialBinding *binding, graph::mtl::SamplerSlot slot) const
    {
        if (!binding)
            return nullptr;

        auto it = domain_resource_bindings.find(binding);
        if (it == domain_resource_bindings.end())
            return nullptr;

        auto r = it->second.find(ToBindingKey(slot));
        if (r == it->second.end())
            return nullptr;

        return &r->second;
    }

    void RenderDescriptorBindingSystem::Update(float /*deltaTime*/)
    {
        SyncBindingsForCurrentCommand(true);
    }

    void RenderDescriptorBindingSystem::Render(graph::RenderCmdBuffer * /*cmd*/, float /*deltaTime*/)
    {
        // Critical for RenderDrawOnly path: Update() is not called there.
        SyncBindingsForCurrentCommand(false);
    }

    void RenderDescriptorBindingSystem::SyncBindingsForCurrentCommand(bool run_contract_diagnostics)
    {
        if (!context)
            return;

        if (run_contract_diagnostics)
            ValidateContractsSideChannel();

        EnsureViewportUBO();
        RefreshSceneUBOResolvers();

        std::unordered_set<const graph::MaterialTemplate *> active_materials;
        ApplyBatchMaterialBindings(active_materials);
        ApplyPipelineMaterialBindings(active_materials);
        ApplyDomainBindings();
        PurgeStaleBindings(active_materials);
    }

    void RenderDescriptorBindingSystem::ApplyBatchMaterialBindings(
        std::unordered_set<const graph::MaterialTemplate *> &out_active)
    {
        if (!context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        std::unordered_set<uint64_t> transform_bound_material_domain_pairs;
        std::unordered_set<uint64_t> transform_id_bound_material_domain_pairs;
        std::unordered_set<uint64_t> mi_bound_material_domain_pairs;
        std::unordered_set<uint64_t> domain_direct_mi_bound_pairs;
        std::unordered_set<uint64_t> domain_direct_mit_bound_pairs;
        graph::BufferManager *buffer_manager = GetBufferManager(context);

        uint32_t domain_direct_mi_hit_count = 0;
        uint32_t domain_direct_mi_fallback_count = 0;
        uint32_t domain_direct_mit_hit_count = 0;
        uint32_t domain_direct_mit_fallback_count = 0;
        uint32_t domain_direct_mit_attempt_count = 0;
        uint32_t domain_direct_mit_semantic_off_count = 0;
        bool domain_direct_mode_seen = false;
        uint32_t domain_direct_mit_reason_counts[static_cast<size_t>(DomainDirectMITBindResult::Count)] = {};

        for (const auto &pair : cache.materialBatches)
        {
            graph::MaterialTemplate *material = pair.first.material;
            if (!material)
                continue;

            out_active.insert(material);

            const MaterialBatch *batch = pair.second.get();
            auto *domain = pair.first.domain;

            if (batch && batch->mi_buffer && batch->mi_buffer->IsUsingResolvedDomainMIID())
                domain_direct_mode_seen = true;

            const auto &contract = material->GetBindingContract();
            ApplySceneUBOBindings(material, contract, scene_ubo_resolvers);

            if (contract.ssbos[size_t(graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID)] == 0)
                ++domain_direct_mit_semantic_off_count;

            for (size_t i = 1; i < graph::mtl::SSBODescriptorSemanticCount; ++i)
            {
                if (contract.ssbos[i] == 0)
                    continue;

                switch (graph::mtl::SSBODescriptorSemantic(i))
                {
                case graph::mtl::SSBODescriptorSemantic::TransformData:
                {
                    if (batch
                     && batch->transform_buffer
                     && material->hasLocalToWorld())
                    {
                        const uint64_t bind_key = (uint64_t(reinterpret_cast<uintptr_t>(material)) << 1)
                                                ^ uint64_t(reinterpret_cast<uintptr_t>(domain));

                        if (!transform_bound_material_domain_pairs.contains(bind_key))
                        {
                            batch->transform_buffer->BindTransform(material);
                            transform_bound_material_domain_pairs.insert(bind_key);
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::TransformID:
                {
                    if (batch && batch->transform_buffer)
                    {
                        const uint64_t bind_key = (uint64_t(reinterpret_cast<uintptr_t>(material)) << 1)
                                                ^ uint64_t(reinterpret_cast<uintptr_t>(domain));

                        if (!transform_id_bound_material_domain_pairs.contains(bind_key))
                        {
                            batch->transform_buffer->BindTransformID(material);
                            transform_id_bound_material_domain_pairs.insert(bind_key);
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceID:
                {
                    if (batch && batch->mi_buffer)
                        batch->mi_buffer->BindMaterialInstanceID(material);
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceData:
                {
                    if (batch
                     && batch->mi_buffer
                     && material->hasMI())
                    {
                        const uint64_t bind_key = (uint64_t(reinterpret_cast<uintptr_t>(material)) << 1)
                                                ^ uint64_t(reinterpret_cast<uintptr_t>(domain));

                        static uint32_t s_mi_bind_tick = 0;
                        const bool log_this = (++s_mi_bind_tick <= 6u);

                        if (!mi_bound_material_domain_pairs.contains(bind_key))
                        {
                            if (log_this)
                                LogDebug("[DescBind::MIDATA] BIND material=%p(%s) domain=%p bind_key=%llu",
                                         (void*)material,
                                         material->GetName().c_str(),
                                         (void*)domain,
                                         static_cast<unsigned long long>(bind_key));

                            bool bound_domain_direct = false;
                            if (!domain_direct_mi_bound_pairs.contains(bind_key))
                            {
                                bound_domain_direct = TryBindDomainDirectMIData(batch, material, domain, buffer_manager);
                                if (bound_domain_direct)
                                {
                                    domain_direct_mi_bound_pairs.insert(bind_key);
                                    ++domain_direct_mi_hit_count;
                                }
                            }

                            // Fallback path: legacy MIAB-owned MaterialInstanceData SSBO
                            if (!bound_domain_direct)
                            {
                                batch->mi_buffer->BindMaterialInstance(material);
                                ++domain_direct_mi_fallback_count;
                            }

                            mi_bound_material_domain_pairs.insert(bind_key);
                        }
                        else if (log_this)
                        {
                            LogDebug("[DescBind::MIDATA] SKIP(dup) material=%p domain=%p bind_key=%llu",
                                     (void*)material,
                                     (void*)domain,
                                     static_cast<unsigned long long>(bind_key));
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID:
                {
                    if (batch && batch->mi_buffer)
                    {
                        const uint64_t bind_key = (uint64_t(reinterpret_cast<uintptr_t>(material)) << 1)
                                                ^ uint64_t(reinterpret_cast<uintptr_t>(domain));

                        bool bound_domain_direct = false;
                        DomainDirectMITBindResult mit_result = DomainDirectMITBindResult::Success;
                        if (!domain_direct_mit_bound_pairs.contains(bind_key))
                        {
                            ++domain_direct_mit_attempt_count;
                            bound_domain_direct = TryBindDomainDirectMITData(batch, material, domain, buffer_manager, &mit_result);
                            if (bound_domain_direct)
                            {
                                domain_direct_mit_bound_pairs.insert(bind_key);
                                ++domain_direct_mit_hit_count;
                            }
                        }

                        if (!bound_domain_direct)
                        {
                            batch->mi_buffer->BindMaterialInstanceTextureID(material);
                            ++domain_direct_mit_fallback_count;

                            const size_t reason_index = static_cast<size_t>(mit_result);
                            if (reason_index < static_cast<size_t>(DomainDirectMITBindResult::Count))
                                ++domain_direct_mit_reason_counts[reason_index];
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }

        }

        // Transitional diagnostics: deterministic summary for domain-direct path verification.
        static uint32_t s_domain_direct_summary_tick = 0;
        if (s_domain_direct_summary_tick < 32)
        {
            std::fprintf(stderr,
                         "[DescBind::DomainDirectSummary] mode_seen=%u batches=%u mi_direct=%u mi_fallback=%u mit_direct=%u mit_fallback=%u mit_attempt=%u mit_semantic_off=%u mit_reason_top=%s:%u\n",
                         domain_direct_mode_seen ? 1u : 0u,
                         static_cast<uint32_t>(cache.materialBatches.size()),
                         domain_direct_mi_hit_count,
                         domain_direct_mi_fallback_count,
                         domain_direct_mit_hit_count,
                         domain_direct_mit_fallback_count,
                         domain_direct_mit_attempt_count,
                         domain_direct_mit_semantic_off_count,
                         ([&]() -> const char *
                          {
                              size_t best_i = 0;
                              uint32_t best_v = 0;
                              for (size_t i = 0; i < static_cast<size_t>(DomainDirectMITBindResult::Count); ++i)
                              {
                                  if (domain_direct_mit_reason_counts[i] > best_v)
                                  {
                                      best_v = domain_direct_mit_reason_counts[i];
                                      best_i = i;
                                  }
                              }
                              return ToString(static_cast<DomainDirectMITBindResult>(best_i));
                          })(),
                         ([&]() -> uint32_t
                          {
                              uint32_t best_v = 0;
                              for (size_t i = 0; i < static_cast<size_t>(DomainDirectMITBindResult::Count); ++i)
                              {
                                  if (domain_direct_mit_reason_counts[i] > best_v)
                                      best_v = domain_direct_mit_reason_counts[i];
                              }
                              return best_v;
                          })());
        }
        ++s_domain_direct_summary_tick;
    }

    void RenderDescriptorBindingSystem::ApplyPipelineMaterialBindings(
        std::unordered_set<const graph::MaterialTemplate *> &out_active)
    {
        for (graph::MaterialTemplate *material : pipeline_materials)
        {
            if (!material)
                continue;

            out_active.insert(material);

            const auto &contract = material->GetBindingContract();
            ApplySceneUBOBindings(material, contract, scene_ubo_resolvers);

            for (size_t i = 1; i < graph::mtl::SSBODescriptorSemanticCount; ++i)
            {
                if (contract.ssbos[i] == 0)
                    continue;

                switch (graph::mtl::SSBODescriptorSemantic(i))
                {
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID:
                {
                    // GraphicsPipeline materials manage their own MIT SSBO via RegisterSceneUBOResolver
                    // or a dedicated registration path — no mi_buffer available here.
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    void RenderDescriptorBindingSystem::PurgeStaleBindings(
        const std::unordered_set<const graph::MaterialTemplate *> &active)
    {
        for (auto it = material_resource_bindings.begin(); it != material_resource_bindings.end();)
        {
            if (active.find(it->first) == active.end())
                it = material_resource_bindings.erase(it);
            else
                ++it;
        }

        for (auto it = contract_last_ok.begin(); it != contract_last_ok.end();)
        {
            if (active.find(it->first) == active.end())
                it = contract_last_ok.erase(it);
            else
                ++it;
        }
    }

    bool RenderDescriptorBindingSystem::IsUBOSemanticResolvable(graph::mtl::UBODescriptorSemantic semantic) const
    {
        if (!context)
            return false;

        if (!RangeCheck(semantic))
            return false;

        auto *accessor = scene_ubo_resolvers[size_t(semantic)];
        return accessor && accessor->GetGPUBuffer() != nullptr;
    }

    bool RenderDescriptorBindingSystem::IsSSBOSemanticResolvable(graph::mtl::SSBODescriptorSemantic semantic) const
    {
        return RangeCheck(semantic);
    }

    void RenderDescriptorBindingSystem::ValidateContractsSideChannel()
    {
        if (!contract_diagnostics_enabled || !context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        ContractDiagStats frame_stats;

        for (const auto &pair : cache.materialBatches)
        {
            const auto &key = pair.first;
            const graph::MaterialTemplate *material = key.material;
            if (!material)
                continue;

            ++frame_stats.materials_checked;

            const auto &contract = material->GetBindingContract();
            bool all_required_ok = true;
            std::string first_error;

            for (size_t i = 1; i < graph::mtl::UBODescriptorSemanticCount; ++i)
            {
                if (contract.ubos[i] == 0)
                    continue;

                const auto ubo_semantic = graph::mtl::UBODescriptorSemantic(i);
                if (IsUBOSemanticResolvable(ubo_semantic))
                    continue;

                ++frame_stats.required_missing;
                all_required_ok = false;

                if (first_error.empty())
                {
                    first_error = "missing semantic=";
                    first_error += graph::mtl::GetUBODescriptorSemanticName(ubo_semantic);
                }
            }

            for (size_t i = 1; i < graph::mtl::SSBODescriptorSemanticCount; ++i)
            {
                if (contract.ssbos[i] == 0)
                    continue;

                const auto ssbo_semantic = graph::mtl::SSBODescriptorSemantic(i);
                if (IsSSBOSemanticResolvable(ssbo_semantic))
                    continue;

                ++frame_stats.required_missing;
                all_required_ok = false;

                if (first_error.empty())
                {
                    first_error = "missing semantic=";
                    first_error += graph::mtl::GetSSBODescriptorSemanticName(ssbo_semantic);
                }
}

            auto it = contract_last_ok.find(material);
            if (it == contract_last_ok.end())
            {
                contract_last_ok.emplace(material, all_required_ok);

                if (!all_required_ok)
                    ++frame_stats.materials_unresolved;

                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s unresolved required contract: %s",
                               material->GetName().c_str(),
                               first_error.c_str());
                }
                continue;
            }

            if (it->second != all_required_ok)
            {
                if (!all_required_ok)
                {
                    LogWarning("[DescriptorContract] material=%s contract changed to unresolved: %s",
                               material->GetName().c_str(),
                               first_error.c_str());
                }
                else
                {
                    LogInfo("[DescriptorContract] material=%s contract resolved", material->GetName().c_str());
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

    void RenderDescriptorBindingSystem::ApplyDomainBindings()
    {
        if (registered_domain_bindings.empty())
            return;

        for (graph::DomainMaterialBinding *binding : registered_domain_bindings)
        {
            if (!binding)
                continue;

            // MaterialInstanceTextureID for domain bindings requires a domain-level
            // MIT buffer — not yet implemented for the domain path.

            binding->Update();
        }
    }
}
