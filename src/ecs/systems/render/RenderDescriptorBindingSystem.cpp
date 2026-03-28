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
        void ApplySceneUBOBindings(graph::Material *material,
                                   const graph::mtl::BindingContract &contract,
                                   const std::array<graph::UBOAccessorBase *, graph::mtl::UBODescriptorSemanticCount> &scene_ubo_resolvers)
        {
            if (!material)
                return;

            for (const auto &[ubo_semantic, stage_flags] : contract.ubos)
            {
                if (ubo_semantic == graph::mtl::UBODescriptorSemantic::Unknown)
                    continue;

                auto *accessor = scene_ubo_resolvers[size_t(ubo_semantic)];
                if (accessor)
                    material->BindUBO(accessor);

                (void)stage_flags;
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
        RefreshSceneUBOResolvers();

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
            ApplySceneUBOBindings(material, contract, scene_ubo_resolvers);

            for (const auto &[ssbo_semantic, stage_flags] : contract.ssbos)
            {
                switch (ssbo_semantic)
                {
                case graph::mtl::SSBODescriptorSemantic::TransformData:
                {
                    if (batch
                     && batch->transform_buffer
                     && material->hasLocalToWorld())
                    {
                        batch->transform_buffer->BindTransform(material);
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::TransformID:
                {
                    if (batch && batch->transform_buffer)
                        batch->transform_buffer->BindTransformID(material);
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
                     && material->hasMI()
                     && !mi_bound_materials.contains(material))
                    {
                        batch->mi_buffer->BindMaterialInstance(material);
                        mi_bound_materials.insert(material);
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID:
                {
                    if (batch && batch->mi_buffer)
                        batch->mi_buffer->BindMaterialInstanceTextureID(material);
                    break;
                }
                default:
                    break;
                }

                (void)stage_flags;
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
            ApplySceneUBOBindings(material, contract, scene_ubo_resolvers);

            for (const auto &[ssbo_semantic, stage_flags] : contract.ssbos)
            {
                switch (ssbo_semantic)
                {
                case graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID:
                {
                    // Pipeline materials manage their own MIT SSBO via RegisterSceneUBOResolver
                    // or a dedicated registration path — no mi_buffer available here.
                    break;
                }
                default:
                    break;
                }

                (void)stage_flags;
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

    bool RenderDescriptorBindingSystem::IsUBOSemanticResolvable(graph::mtl::UBODescriptorSemantic semantic) const
    {
        if (!context)
            return false;

        if (semantic == graph::mtl::UBODescriptorSemantic::Unknown)
            return false;

        auto *accessor = scene_ubo_resolvers[size_t(semantic)];
        return accessor && accessor->GetGPUBuffer() != nullptr;
    }

    bool RenderDescriptorBindingSystem::IsSSBOSemanticResolvable(graph::mtl::SSBODescriptorSemantic semantic) const
    {
        switch (semantic)
        {
        case graph::mtl::SSBODescriptorSemantic::TransformData:
        case graph::mtl::SSBODescriptorSemantic::TransformID:
        case graph::mtl::SSBODescriptorSemantic::MaterialInstanceID:
        case graph::mtl::SSBODescriptorSemantic::MaterialInstanceData:
        case graph::mtl::SSBODescriptorSemantic::MaterialInstanceTextureID:
            // Resolved per-batch inside ApplyBatchMaterialBindings; not checkable here.
            return false;
        case graph::mtl::SSBODescriptorSemantic::Custom:
            return true;

        case graph::mtl::SSBODescriptorSemantic::Unknown:
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

            auto validate_ubo_requirements = [&](const std::map<graph::mtl::UBODescriptorSemantic, uint32_t> &requirements)
            {
                for (const auto &[ubo_semantic, stage_flags] : requirements)
                {
                    const bool resolvable = IsUBOSemanticResolvable(ubo_semantic);
                    if (resolvable)
                        continue;

                    ++frame_stats.required_missing;
                    all_required_ok = false;

                    if (first_error.empty())
                    {
                        first_error = "missing semantic=";
                        first_error += graph::mtl::GetUBODescriptorSemanticName(ubo_semantic);
                    }

                    (void)stage_flags;
                }
            };

            auto validate_ssbo_requirements = [&](const std::map<graph::mtl::SSBODescriptorSemantic, uint32_t> &requirements)
            {
                for (const auto &[ssbo_semantic, stage_flags] : requirements)
                {
                    const bool resolvable = IsSSBOSemanticResolvable(ssbo_semantic);
                    if (resolvable)
                        continue;

                    ++frame_stats.required_missing;
                    all_required_ok = false;

                    if (first_error.empty())
                    {
                        first_error = "missing semantic=";
                        first_error += graph::mtl::GetSSBODescriptorSemanticName(ssbo_semantic);
                    }

                    (void)stage_flags;
                }
            };

            validate_ubo_requirements(contract.ubos);
            validate_ssbo_requirements(contract.ssbos);

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
