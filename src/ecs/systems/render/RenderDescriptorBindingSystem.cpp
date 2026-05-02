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
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKBufferOwner.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/common/MeshShaderStreamContract.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<unordered_set>
#include<cstdlib>
#include<string>

namespace hgl::ecs
{
    namespace
    {
        graph::DomainResourceBinding *ResolveDomainBinding(hgl::ecs::ECSContext *context,
                                                           graph::ResourceDomain *domain,
                                                           graph::ShaderMaterialProgram *material)
        {
            if (!context || !domain || !material)
                return nullptr;

            auto *graphics_context = context->GetGraphicsContext();
            auto *material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr;
            return material_manager ? material_manager->FindDomainMaterialBinding(domain, material) : nullptr;
        }

        void ApplySceneUBOBindings(graph::ShaderMaterialProgram *material,
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

        bool MaterialHasMeshOrTaskStages(graph::ShaderMaterialProgram *material)
        {
            if (!material)
                return false;

            for (const auto &stage_ci : material->GetStageList())
            {
                if (stage_ci.stage == VK_SHADER_STAGE_TASK_BIT_EXT
                 || stage_ci.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
                    return true;
            }

            return false;
        }

        bool ResolveUniformVertexStreamFromBatch(const MaterialBatch *batch,
                                                 graph::AttributeSemantic semantic,
                                                 const graph::IGPUBuffer *&out_gpu,
                                                 VkDeviceSize &out_offset,
                                                 VkDeviceSize &out_stride)
        {
            out_gpu = nullptr;
            out_offset = 0;
            out_stride = 0;

            if (!batch)
                return false;

            bool has_value = false;

            for (auto *item : batch->items)
            {
                if (!item)
                    continue;

                auto *primitive = item->GetPrimitive();
                if (!primitive)
                    continue;

                const graph::IGPUBuffer *gpu = nullptr;
                VkDeviceSize offset = 0;
                VkDeviceSize stride = 0;

                if (!primitive->ResolveVertexStreamSource(semantic, gpu, offset, stride) || !gpu)
                    return false;

                if (!has_value)
                {
                    out_gpu = gpu;
                    out_offset = offset;
                    out_stride = stride;
                    has_value = true;
                    continue;
                }

                if (out_gpu != gpu || out_offset != offset || out_stride != stride)
                    return false;
            }

            return has_value;
        }

        bool ResolveUniformMeshIndexStreamFromBatch(const MaterialBatch *batch,
                                                    const graph::IGPUBuffer *&out_gpu)
        {
            out_gpu = nullptr;

            if (!batch)
                return false;

            bool has_value = false;

            for (auto *item : batch->items)
            {
                if (!item)
                    continue;

                auto *primitive = item->GetPrimitive();
                if (!primitive)
                    continue;

                const graph::IGPUBuffer *gpu = primitive->ResolveMeshIndexStreamSource();
                if (!gpu)
                    return false;

                if (!has_value)
                {
                    out_gpu = gpu;
                    has_value = true;
                    continue;
                }

                if (out_gpu != gpu)
                    return false;
            }

            return has_value;
        }

        void ApplyAutoVertexStreamsBinding(graph::ShaderMaterialProgram *material,
                                           const MaterialBatch *batch)
        {
            if (!material || !batch)
                return;

            auto *mp = material->GetMP(graph::DescriptorSetType::VertexStreams);
            if (!mp)
                return;

              for (uint32_t binding = graph::kVertexStreamAttributeBindingBegin;
                  binding < graph::kVertexStreamAttributeBindingCount;
                 ++binding)
            {
                if (!mp->HasBinding(binding))
                    continue;

                const auto semantic = graph::AttributeSemantic(binding);

                const graph::IGPUBuffer *gpu = nullptr;
                VkDeviceSize offset = 0;
                VkDeviceSize stride = 0;

                if (!ResolveUniformVertexStreamFromBatch(batch, semantic, gpu, offset, stride) || !gpu)
                    continue;

                material->BindAttribStream(semantic, gpu, uint32_t(offset), uint32_t(stride));
            }

            if (mp->HasBinding(graph::kVertexStreamPositionBinding))
            {
                const graph::IGPUBuffer *gpu = nullptr;
                VkDeviceSize offset = 0;
                VkDeviceSize stride = 0;

                if (ResolveUniformVertexStreamFromBatch(batch, graph::AttributeSemantic::BuiltinCount, gpu, offset, stride) && gpu)
                    mp->BindVertexStreamSSBO(graph::kVertexStreamPositionBinding, gpu);
            }

            if (MaterialHasMeshOrTaskStages(material) && mp->HasBinding(graph::kMeshShaderIndexStreamBinding))
            {
                const graph::IGPUBuffer *mesh_index_gpu = nullptr;
                if (ResolveUniformMeshIndexStreamFromBatch(batch, mesh_index_gpu) && mesh_index_gpu)
                    material->BindMeshIndexStream(mesh_index_gpu);
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

    bool RenderDescriptorBindingSystem::RegisterMaterialTexture(graph::ShaderMaterialProgram *material,
                                                                graph::mtl::SamplerSlot slot,
                                                                graph::Texture *texture)
    {
        if (!material || !texture)
            return false;

        auto &binding = material_resource_bindings[material][ToBindingKey(slot)];
        binding.texture = texture;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTextureSampler(graph::ShaderMaterialProgram *material,
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

    void RenderDescriptorBindingSystem::RemoveMaterialBinding(graph::ShaderMaterialProgram *material, graph::mtl::SamplerSlot slot)
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

    void RenderDescriptorBindingSystem::ClearMaterialBindings(graph::ShaderMaterialProgram *material)
    {
        if (!material)
            return;

        material_resource_bindings.erase(material);
    }

    void RenderDescriptorBindingSystem::RegisterPipelineMaterial(graph::ShaderMaterialProgram *material)
    {
        if (material)
            pipeline_materials.insert(material);
    }

    void RenderDescriptorBindingSystem::UnregisterPipelineMaterial(graph::ShaderMaterialProgram *material)
    {
        if (material)
            pipeline_materials.erase(material);
    }

    // Phase 2 — DomainResourceBinding registration

    void RenderDescriptorBindingSystem::RegisterDomainBinding(graph::DomainResourceBinding *binding)
    {
        if (binding)
            registered_domain_bindings.insert(binding);
    }

    void RenderDescriptorBindingSystem::UnregisterDomainBinding(graph::DomainResourceBinding *binding)
    {
        if (!binding)
            return;
        registered_domain_bindings.erase(binding);
        domain_resource_bindings.erase(binding);
    }

    bool RenderDescriptorBindingSystem::RegisterDomainTexture(graph::DomainResourceBinding *binding,
                                                              graph::mtl::SamplerSlot slot,
                                                              graph::Texture *tex)
    {
        if (!binding || !tex)
            return false;
        auto &binding_slot = domain_resource_bindings[binding][ToBindingKey(slot)];
        binding_slot.texture = tex;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterDomainTextureSampler(graph::DomainResourceBinding *binding,
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

    void RenderDescriptorBindingSystem::ClearDomainBindings(graph::DomainResourceBinding *binding)
    {
        if (binding)
            domain_resource_bindings.erase(binding);
    }

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindMaterialResourceBinding(const graph::ShaderMaterialProgram *material,
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
        const graph::DomainResourceBinding *binding, graph::mtl::SamplerSlot slot) const
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

        std::unordered_set<const graph::ShaderMaterialProgram *> active_materials;
        ApplyBatchMaterialBindings(active_materials);
        ApplyPipelineMaterialBindings(active_materials);
        ApplyDomainBindings();
        PurgeStaleBindings(active_materials);
    }

    void RenderDescriptorBindingSystem::ApplyBatchMaterialBindings(
        std::unordered_set<const graph::ShaderMaterialProgram *> &out_active)
    {
        if (!context)
            return;

        const auto &cache = context->GetRenderFrameCache();
        std::unordered_set<const void *> mi_bound_targets;

        for (const auto &pair : cache.materialBatches)
        {
            graph::ShaderMaterialProgram *material = pair.first.material;
            if (!material)
                continue;

            out_active.insert(material);

            const MaterialBatch *batch = pair.second.get();
            graph::DomainResourceBinding *domain_binding = ResolveDomainBinding(context, pair.first.domain, material);
            graph::MaterialParameters *domain_mp = domain_binding ? domain_binding->GetPerMaterialMP() : nullptr;
            graph::MaterialParameters *domain_po = domain_binding ? domain_binding->GetPerObjectMP()   : nullptr;

            const auto &contract = material->GetBindingContract();
            ApplySceneUBOBindings(material, contract, scene_ubo_resolvers);
            ApplyAutoVertexStreamsBinding(material, batch);

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
                        if (domain_po)
                        {
                            auto *tab_buf = batch->transform_buffer->GetTransformDataBuffer();
                            auto *tab_gpu = tab_buf ? tab_buf->GetGPUBuffer() : nullptr;
                            if (tab_gpu)
                                domain_po->BindSSBO(graph::mtl::SSBODescriptorSemantic::TransformData, tab_gpu);
                        }
                        else
                        {
                            batch->transform_buffer->BindTransform(material);
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::TransformID:
                {
                    if (batch && batch->transform_buffer)
                    {
                        if (domain_po)
                        {
                            auto *tid_buf = batch->transform_buffer->GetTransformIDDataBuffer();
                            auto *tid_gpu = tid_buf ? tid_buf->GetGPUBuffer() : nullptr;
                            if (tid_gpu)
                                domain_po->BindSSBO(graph::mtl::SSBODescriptorSemantic::TransformID, tid_gpu);
                        }
                        else
                        {
                            batch->transform_buffer->BindTransformID(material);
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceID:
                {
                    if (batch && batch->mi_buffer)
                    {
                        if (domain_mp)
                        {
                            auto *id_buf = batch->mi_buffer->GetMaterialInstanceIDBuffer();
                            auto *id_gpu = id_buf ? id_buf->GetGPUBuffer() : nullptr;
                            if (id_gpu)
                                domain_mp->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceID, id_gpu);
                        }
                        else
                        {
                            batch->mi_buffer->BindMaterialInstanceID(material);
                        }
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData:
                {
                    const void *mi_bind_target = domain_mp
                        ? static_cast<const void *>(domain_mp)
                        : static_cast<const void *>(material);

                    if (batch
                     && batch->mi_buffer
                     && material->hasMI()
                     && !mi_bound_targets.contains(mi_bind_target))
                    {
                        if (domain_mp)
                        {
                            auto *mi_buf = batch->mi_buffer->GetMaterialInstanceBuffer();
                            auto *mi_gpu = mi_buf ? mi_buf->GetGPUBuffer() : nullptr;
                            if (mi_gpu)
                                domain_mp->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceData, mi_gpu);
                        }
                        else
                        {
                            batch->mi_buffer->BindMaterialInstance(material);
                        }

                        mi_bound_targets.insert(mi_bind_target);
                    }
                    break;
                }
                case graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceTexture:
                {
                    if (batch && batch->mi_buffer)
                    {
                        if (domain_mp)
                        {
                            auto *mit_buf = batch->mi_buffer->GetMaterialInstanceTextureIDBuffer();
                            auto *mit_gpu = mit_buf ? mit_buf->GetGPUBuffer() : nullptr;
                            if (mit_gpu)
                            {
                                domain_mp->BindSSBO(graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceTexture, mit_gpu);
                                LogInfo("[RenderDescriptorBindingSystem] domain MI texture bind material=%s domain=%p gpu=%p",
                                        material->GetName().c_str(),
                                        static_cast<void *>(pair.first.domain),
                                        static_cast<void *>(mit_gpu));
                            }
                        }
                        else
                        {
                            batch->mi_buffer->BindMaterialInstanceTextureID(material);
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }

        }
    }

    void RenderDescriptorBindingSystem::ApplyPipelineMaterialBindings(
        std::unordered_set<const graph::ShaderMaterialProgram *> &out_active)
    {
        for (graph::ShaderMaterialProgram *material : pipeline_materials)
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
                case graph::mtl::SSBODescriptorSemantic::MaterialBindingInstanceTexture:
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
        const std::unordered_set<const graph::ShaderMaterialProgram *> &active)
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
            const graph::ShaderMaterialProgram *material = key.material;
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
            if (frame_stats.materials_unresolved > 0 || frame_stats.required_missing > 0)
            {
                LogWarning("[DescriptorContract] frame stats: checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u",
                           frame_stats.materials_checked,
                           frame_stats.materials_unresolved,
                           frame_stats.required_missing,
                           frame_stats.optional_missing,
                           frame_stats.fallback_hits);
            }
            else
            {
                LogDebug("[DescriptorContract] frame stats: checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u",
                         frame_stats.materials_checked,
                         frame_stats.materials_unresolved,
                         frame_stats.required_missing,
                         frame_stats.optional_missing,
                         frame_stats.fallback_hits);
            }

            last_contract_stats = frame_stats;
        }
    }

    void RenderDescriptorBindingSystem::ApplyDomainBindings()
    {
        if (registered_domain_bindings.empty())
            return;

        for (graph::DomainResourceBinding *binding : registered_domain_bindings)
        {
            if (!binding)
                continue;

            // MaterialBindingInstanceTexture for domain bindings requires a domain-level
            // MIT buffer — not yet implemented for the domain path.

            binding->Update();
        }
    }
}
