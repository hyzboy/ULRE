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
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<unordered_set>
#include<cstdlib>
#include<string>

namespace hgl::ecs
{
    namespace
    {
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
        using S = graph::mtl::DescriptorSemantic;

        scene_ubo_resolvers[S::ViewportInfo] = [this]() -> const graph::IGPUBuffer* {
            return viewport_ubo ? viewport_ubo->GetGPUBuffer() : nullptr;
        };

        scene_ubo_resolvers[S::CameraInfo] = [this]() -> const graph::IGPUBuffer* {
            if (!context) return nullptr;
            auto cs = context->GetSystem<CameraSystem>();
            if (!cs) return nullptr;
            auto *ubo = cs->GetCameraUBO();
            return ubo ? ubo->GetGPUBuffer() : nullptr;
        };

        scene_ubo_resolvers[S::SkyInfo] = [this]() -> const graph::IGPUBuffer* {
            if (!context) return nullptr;
            auto es = context->GetSystem<EnvironmentSystem>();
            if (!es) {
                es = context->RegisterRenderSystem<EnvironmentSystem>();
                if (es && context->IsActive()) {
                    es->OnDependenciesReady();
                    es->Initialize();
                }
            }
            if (!es) return nullptr;
            es->EditSkyInfo();
            auto *ubo = es->GetSkyUBO();
            return ubo ? ubo->GetGPUBuffer() : nullptr;
        };
    }

    void RenderDescriptorBindingSystem::RegisterSceneUBOResolver(graph::mtl::DescriptorSemantic semantic, SceneUBOResolver resolver)
    {
        if (resolver)
            scene_ubo_resolvers[semantic] = std::move(resolver);
    }

    void RenderDescriptorBindingSystem::UnregisterSceneUBOResolver(graph::mtl::DescriptorSemantic semantic)
    {
        scene_ubo_resolvers.erase(semantic);
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

    bool RenderDescriptorBindingSystem::RegisterMaterialTexture(graph::Material *material,
                                                                graph::mtl::SamplerSlot slot,
                                                                graph::Texture *texture)
    {
        if (!material || !texture)
            return false;

        auto &binding = material_resource_bindings[material][ToBindingKey(slot)];
        binding.texture = texture;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTextureSampler(graph::Material *material,
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

    void RenderDescriptorBindingSystem::RemoveMaterialBinding(graph::Material *material, graph::mtl::SamplerSlot slot)
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

    void RenderDescriptorBindingSystem::ClearMaterialBindings(graph::Material *material)
    {
        if (!material)
            return;

        material_resource_bindings.erase(material);
    }

    void RenderDescriptorBindingSystem::RegisterPipelineMaterial(graph::Material *material)
    {
        if (material)
            pipeline_materials.insert(material);
    }

    void RenderDescriptorBindingSystem::UnregisterPipelineMaterial(graph::Material *material)
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

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindMaterialResourceBinding(const graph::Material *material,
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

        std::unordered_set<const graph::Material *> active_materials;
        ApplyBatchMaterialBindings(active_materials);
        ApplyPipelineMaterialBindings(active_materials);
        ApplyDomainBindings();
        PurgeStaleBindings(active_materials);
    }



    void RenderDescriptorBindingSystem::ApplyBatchMaterialBindings(
        std::unordered_set<const graph::Material *> &out_active)
    {
        if (!context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        std::unordered_set<graph::Material *> mi_bound_materials;

        for (const auto &pair : cache.materialBatches)
        {
            graph::Material *material = pair.first.material;
            if (!material)
                continue;

            out_active.insert(material);

            const MaterialBatch *batch = pair.second.get();

            const auto &contract = material->GetBindingContract();
            for (const auto &req : contract.requirements)
            {
                const auto resolved = graph::mtl::ResolveDescriptorRequirement(req);
                if (!resolved.name || !*resolved.name)
                    continue;

                // Scene UBO resolver lookup
                auto resolver_it = scene_ubo_resolvers.find(resolved.semantic);
                if (resolver_it != scene_ubo_resolvers.end())
                {
                    const auto *buf = resolver_it->second();
                    if (buf) material->BindUBO(resolved.set_type, resolved.name, buf, false);
                    continue;
                }

                switch (resolved.semantic)
                {
                case graph::mtl::DescriptorSemantic::LocalToWorld:
                {
                    if (batch
                     && batch->transform_buffer
                     && material->hasLocalToWorld())
                    {
                        batch->transform_buffer->BindTransform(material);
                    }
                    break;
                }
                case graph::mtl::DescriptorSemantic::TransformID:
                {
                    if (batch && batch->transform_buffer)
                        batch->transform_buffer->BindTransformID(material);
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialInstanceID:
                {
                    if (batch && batch->mi_buffer)
                        batch->mi_buffer->BindMaterialInstanceID(material);
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialInstance:
                {
                    if (batch
                     && batch->mi_buffer
                     && material->hasMI()
                     && !mi_bound_materials.contains(material))
                    {
                        batch->mi_buffer->BindMaterialInstance(material);
                        mi_bound_materials.insert(material);
                    }
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialInstanceTextureID:
                {
                    if (batch && batch->mi_buffer)
                        batch->mi_buffer->BindMaterialInstanceTextureID(material);
                    break;
                }
                default:
                    break;
                }
            }

        }
    }

    void RenderDescriptorBindingSystem::ApplyPipelineMaterialBindings(
        std::unordered_set<const graph::Material *> &out_active)
    {
        for (graph::Material *material : pipeline_materials)
        {
            if (!material)
                continue;

            out_active.insert(material);

            const auto &contract = material->GetBindingContract();
            for (const auto &req : contract.requirements)
            {
                const auto resolved = graph::mtl::ResolveDescriptorRequirement(req);
                if (!resolved.name || !*resolved.name)
                    continue;

                // Scene UBO resolver lookup
                auto resolver_it = scene_ubo_resolvers.find(resolved.semantic);
                if (resolver_it != scene_ubo_resolvers.end())
                {
                    const auto *buf = resolver_it->second();
                    if (buf) material->BindUBO(resolved.set_type, resolved.name, buf, false);
                    continue;
                }

                switch (resolved.semantic)
                {
                case graph::mtl::DescriptorSemantic::MaterialInstanceTextureID:
                {
                    // Pipeline materials manage their own MIT SSBO via RegisterSceneUBOResolver
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
        const std::unordered_set<const graph::Material *> &active)
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

    bool RenderDescriptorBindingSystem::IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const
    {
        if (!context)
            return false;

        // Check registered scene UBO resolvers first
        auto it = scene_ubo_resolvers.find(semantic);
        if (it != scene_ubo_resolvers.end())
            return it->second && it->second() != nullptr;

        switch (semantic)
        {
        case graph::mtl::DescriptorSemantic::LocalToWorld:
        case graph::mtl::DescriptorSemantic::TransformID:
        case graph::mtl::DescriptorSemantic::MaterialInstanceID:
        case graph::mtl::DescriptorSemantic::MaterialInstance:
        case graph::mtl::DescriptorSemantic::MaterialInstanceTextureID:
            // Resolved per-batch inside ApplyBatchMaterialBindings; not checkable here.
            return false;
        case graph::mtl::DescriptorSemantic::Custom:
            return true;

        case graph::mtl::DescriptorSemantic::Unknown:
        default:
            return false;
        }
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
            const graph::Material *material = key.material;
            if (!material)
                continue;

            ++frame_stats.materials_checked;

            const auto &contract = material->GetBindingContract();
            bool all_required_ok = true;
            std::string first_error;

            for (const auto &req : contract.requirements)
            {
                const auto resolved = graph::mtl::ResolveDescriptorRequirement(req);
                const bool resolvable = IsSemanticResolvable(resolved.semantic);
                if (resolvable)
                    continue;

                if (resolved.required && !resolved.allow_fallback)
                {
                    ++frame_stats.required_missing;
                    all_required_ok = false;

                    if (first_error.empty())
                    {
                        first_error = "missing semantic=";
                        first_error += graph::mtl::GetDescriptorSemanticName(resolved.semantic);
                    }
                }
                else
                {
                    ++frame_stats.optional_missing;

                    if (resolved.allow_fallback)
                        ++frame_stats.fallback_hits;
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
