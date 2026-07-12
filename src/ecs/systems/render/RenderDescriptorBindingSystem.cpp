#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
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
#include<unordered_set>
#include<cstdlib>
#include<string>

namespace hgl::ecs
{
    namespace
    {
        std::string ToBindingKey(const char *name)
        {
            return name ? std::string(name) : std::string();
        }

        std::string ToBindingKey(const AnsiString &name)
        {
            return ToBindingKey(name.c_str());
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
    }

    RenderDescriptorBindingSystem::RenderDescriptorBindingSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameBusinessSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();

        if (const char *env = std::getenv("HGL_ECS_DISABLE_LEGACY_BINDING_FALLBACK"))
        {
            if (*env == '1' || *env == 'y' || *env == 'Y' || *env == 't' || *env == 'T')
                enable_legacy_material_binding_fallback = false;
        }

        EnsureMaterializationCallbacks();
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

    bool RenderDescriptorBindingSystem::RegisterMaterialTexture(graph::Material *material,
                                                                const AnsiString &name,
                                                                graph::Texture *texture)
    {
        if (!material || name.IsEmpty() || !texture)
            return false;

        auto &slot = material_resource_bindings[material][ToBindingKey(name)];
        slot.texture = texture;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTextureSampler(graph::Material *material,
                                                                       const AnsiString &name,
                                                                       graph::Texture *texture,
                                                                       graph::Sampler *sampler)
    {
        if (!RegisterMaterialTexture(material, name, texture))
            return false;

        if (!sampler)
            return false;

        auto &slot = material_resource_bindings[material][ToBindingKey(name)];
        slot.sampler = sampler;
        return true;
    }

    void RenderDescriptorBindingSystem::RemoveMaterialBinding(graph::Material *material, const AnsiString &name)
    {
        if (!material || name.IsEmpty())
            return;

        auto material_it = material_resource_bindings.find(material);
        if (material_it == material_resource_bindings.end())
            return;

        material_it->second.erase(ToBindingKey(name));
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

    bool RenderDescriptorBindingSystem::RegisterMaterialStructLayout(const std::string &struct_name,
                                                                     graph::mtl::SSBOCategory category,
                                                                     uint32_t byte_stride)
    {
        return materialization_struct_pool.RegisterLayout(struct_name, category, byte_stride);
    }

    void RenderDescriptorBindingSystem::ResetMaterializationFrameData()
    {
        materialization_struct_pool.ResetAllocations();
        materialization_index_tables.Clear();
    }

    bool RenderDescriptorBindingSystem::ResolveMaterialRecipe(const graph::mtl::MaterialRecipe &recipe,
                                                              graph::mtl::MaterializationSpec &out_spec,
                                                              uint32_t *out_texture_layer_row,
                                                              uint32_t *out_data_index_row)
    {
        if (context)
        {
            const uint32_t frame_index = context->GetFrameIndex();
            if (materialization_last_reset_frame != frame_index)
            {
                ResetMaterializationFrameData();
                materialization_last_reset_frame = frame_index;
            }
        }

        EnsureMaterializationCallbacks();

        if (!graph::mtl::ResolveMaterializationSpec(recipe, materialization_callbacks, out_spec))
            return false;

        uint32_t texture_row = 0;
        uint32_t data_row = 0;
        if (!graph::mtl::WriteSpecToIndexTables(out_spec, materialization_index_tables, texture_row, data_row))
            return false;

        if (out_texture_layer_row)
            *out_texture_layer_row = texture_row;

        if (out_data_index_row)
            *out_data_index_row = data_row;

        return true;
    }

    bool RenderDescriptorBindingSystem::GetMaterializationPoolStats(uint32_t &texture_count,
                                                                    uint32_t &struct_layout_count,
                                                                    uint32_t &texture_layer_rows,
                                                                    uint32_t &data_index_rows) const
    {
        texture_count = static_cast<uint32_t>(materialization_texture_pool.GetCount());
        struct_layout_count = static_cast<uint32_t>(materialization_struct_pool.GetLayoutCount());
        texture_layer_rows = static_cast<uint32_t>(materialization_index_tables.GetTextureLayerRowCount());
        data_index_rows = static_cast<uint32_t>(materialization_index_tables.GetDataIndexRowCount());
        return true;
    }

    void RenderDescriptorBindingSystem::EnsureMaterializationCallbacks()
    {
        if (!materialization_callbacks.resolve_texture || !materialization_callbacks.resolve_struct)
            materialization_callbacks = graph::mtl::MakePoolResolveCallbacks(materialization_texture_pool, materialization_struct_pool);
    }

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindMaterialResourceBinding(const graph::Material *material, const char *name) const
    {
        if (!material || !name || !*name)
            return nullptr;

        auto material_it = material_resource_bindings.find(material);
        if (material_it == material_resource_bindings.end())
            return nullptr;

        auto resource_it = material_it->second.find(ToBindingKey(name));
        if (resource_it == material_it->second.end())
            return nullptr;

        return &resource_it->second;
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

        const uint32_t frame_index = context->GetFrameIndex();
        if (materialization_last_reset_frame != frame_index)
        {
            ResetMaterializationFrameData();
            materialization_last_reset_frame = frame_index;
        }

        if (run_contract_diagnostics)
            ValidateContractsSideChannel();

        EnsureViewportUBO();

        ApplyContractBindings();
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

    void RenderDescriptorBindingSystem::ApplyContractBindings()
    {
        if (!context)
            return;

        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();

        const auto &cache = context->GetRenderFrameCache();

        std::unordered_set<graph::Material *> l2w_bound_materials;
        std::unordered_set<graph::Material *> mi_bound_materials;
        std::unordered_set<const graph::Material *> active_materials;

        for (const auto &pair : cache.materialBatches)
        {
            graph::Material *material = pair.first.material;
            if (!material)
                continue;

            active_materials.insert(material);

            const MaterialBatch *batch = pair.second.get();

            const auto &contract = material->GetBindingContract();

            for (const auto &req : contract.requirements)
            {
                if (!req.name || !*req.name)
                    continue;

                switch (req.semantic)
                {
                case graph::mtl::DescriptorSemantic::ViewportInfo:
                {
                    const auto *ubo = ResolveViewportUBO();
                    if (ubo)
                        material->BindUBO(req.set_type, req.name, ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::CameraInfo:
                {
                    if (camera_ubo)
                        material->BindUBO(req.set_type, req.name, camera_ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::SkyInfo:
                {
                    if (sky_ubo)
                        material->BindUBO(req.set_type, req.name, sky_ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::LocalToWorld:
                {
                    if (batch
                     && batch->transform_buffer
                     && material->hasLocalToWorld()
                     && !l2w_bound_materials.contains(material))
                    {
                        batch->transform_buffer->BindTransform(material);
                        l2w_bound_materials.insert(material);
                    }
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
                case graph::mtl::DescriptorSemantic::MaterialTexture:
                {
                    const auto *binding = FindMaterialResourceBinding(material, req.name);
                    if (binding && binding->texture)
                        material->BindTexture(req.set_type, req.name, binding->texture);
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialSampler:
                {
                    const auto *binding = FindMaterialResourceBinding(material, req.name);
                    if (binding && binding->texture && binding->sampler)
                        material->BindTextureSampler(req.set_type, req.name, binding->texture, binding->sampler);
                    break;
                }
                default:
                    break;
                }
            }

            // Legacy compatibility fallback:
            // Some existing materials may still rely on hasLocalToWorld/hasMI flags
            // without fully populated contract semantics.
            if (enable_legacy_material_binding_fallback)
            {
                if (batch
                 && batch->transform_buffer
                 && material->hasLocalToWorld()
                 && !l2w_bound_materials.contains(material))
                {
                    batch->transform_buffer->BindTransform(material);
                    l2w_bound_materials.insert(material);
                }

                if (batch
                 && batch->mi_buffer
                 && material->hasMI()
                 && !mi_bound_materials.contains(material))
                {
                    batch->mi_buffer->BindMaterialInstance(material);
                    mi_bound_materials.insert(material);
                }
            }
        }

        // Bind scene-level UBOs to pipeline-registered materials (Line, Terrain, etc.)
        // These materials bypass the normal materialBatches path.
        for (graph::Material *material : pipeline_materials)
        {
            if (!material)
                continue;

            active_materials.insert(material);

            const auto &contract = material->GetBindingContract();

            for (const auto &req : contract.requirements)
            {
                if (!req.name || !*req.name)
                    continue;

                switch (req.semantic)
                {
                case graph::mtl::DescriptorSemantic::ViewportInfo:
                {
                    const auto *ubo = ResolveViewportUBO();
                    if (ubo)
                        material->BindUBO(req.set_type, req.name, ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::CameraInfo:
                {
                    if (camera_ubo)
                        material->BindUBO(req.set_type, req.name, camera_ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::SkyInfo:
                {
                    if (sky_ubo)
                        material->BindUBO(req.set_type, req.name, sky_ubo, false);
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialTexture:
                {
                    const auto *binding = FindMaterialResourceBinding(material, req.name);
                    if (binding && binding->texture)
                        material->BindTexture(req.set_type, req.name, binding->texture);
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialSampler:
                {
                    const auto *binding = FindMaterialResourceBinding(material, req.name);
                    if (binding && binding->texture && binding->sampler)
                        material->BindTextureSampler(req.set_type, req.name, binding->texture, binding->sampler);
                    break;
                }
                default:
                    break;
                }
            }
        }

        for (auto it = material_resource_bindings.begin(); it != material_resource_bindings.end();)
        {
            if (active_materials.find(it->first) == active_materials.end())
                it = material_resource_bindings.erase(it);
            else
                ++it;
        }

        for (auto it = contract_last_ok.begin(); it != contract_last_ok.end();)
        {
            if (active_materials.find(it->first) == active_materials.end())
                it = contract_last_ok.erase(it);
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
        case graph::mtl::DescriptorSemantic::MaterialInstance:
        case graph::mtl::DescriptorSemantic::MaterialTexture:
        case graph::mtl::DescriptorSemantic::MaterialSampler:
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
}
