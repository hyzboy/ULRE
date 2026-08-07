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
        void ResetMaterialRecipe(graph::mtl::MaterialRecipe &recipe)
        {
            recipe.recipe_name.clear();
            recipe.mtl_def_id.clear();
            recipe.domain.clear();
            recipe.vertex_node_config = graph::mtl::MakeDefault3DNodeConfig();
            recipe.material_lod = 0;
            recipe.double_sided = false;
            recipe.alpha_test = false;
            recipe.alpha_cutoff = 0.5f;
            recipe.pipeline_config = graph::mtl::MaterialPipelineConfig{};
            recipe.textures.clear();
            recipe.ssbo_assets.clear();
        }

        std::string ToBindingKey(const char *name)
        {
            return name ? std::string(name) : std::string();
        }

        std::string ToBindingKey(const AnsiString &name)
        {
            return ToBindingKey(name.c_str());
        }

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
    }

    RenderDescriptorBindingSystem::RenderDescriptorBindingSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderFrameBusinessSyncSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
        AddDependency<CameraSystem>();

        EnsureMaterializationCallbacks();
    }

    RenderDescriptorBindingSystem::~RenderDescriptorBindingSystem()
    {
        ReleaseMaterializationIndexBuffers();
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

    bool RenderDescriptorBindingSystem::RegisterMaterialTexture(graph::ShaderProgram *material,
                                                                const AnsiString &name,
                                                                graph::Texture *texture)
    {
        if (!material || name.IsEmpty() || !texture)
            return false;

        auto &slot = material_resource_bindings[material][ToBindingKey(name)];
        slot.texture = texture;
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterMaterialTextureSampler(graph::ShaderProgram *material,
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

    void RenderDescriptorBindingSystem::RemoveMaterialBinding(graph::ShaderProgram *material, const AnsiString &name)
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

    void RenderDescriptorBindingSystem::ClearMaterialBindings(graph::ShaderProgram *material)
    {
        if (!material)
            return;

        material_resource_bindings.erase(material);
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

        return materialization_struct_pool.RegisterLayout(ssbo_type, ssbo_id, byte_stride);
    }

    void RenderDescriptorBindingSystem::ResetMaterializationFrameData()
    {
        materialization_index_tables.Clear();
        materialization_resolve_cache.clear();
        materialization_index_tables_dirty = true;
    }

    bool RenderDescriptorBindingSystem::WriteTextureLayerRowAt(uint32_t at_index,
                                                               const graph::mtl::MaterializationSpec &spec)
    {
        materialization_index_tables.WriteTextureLayerRowAt(at_index, graph::mtl::BuildTextureLayerRow(spec));
        materialization_index_tables_dirty = true;
        return true;
    }

    bool RenderDescriptorBindingSystem::ResolveMaterialRecipe(const graph::mtl::MaterialRecipe &recipe,
                                                              graph::mtl::MaterializationSpec &out_spec,
                                                              uint32_t *out_texture_layer_row,
                                                              uint32_t *out_data_index_row,
                                                              graph::mtl::MaterializationInstanceData *out_instance_data)
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

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(recipe);
        GLogVerbose("[ResolveMaterialRecipe] recipe=%s tex=%zu ssbo=%zu hash=%llu",
                 recipe.recipe_name.c_str(),
                 recipe.textures.size(),
                 recipe.ssbo_assets.size(),
                 static_cast<unsigned long long>(recipe_hash));

        auto cache_it = materialization_resolve_cache.find(recipe_hash);
        if (cache_it != materialization_resolve_cache.end())
        {
            out_spec = graph::mtl::MaterializeMaterializationInstance(cache_it->second.shared_spec, recipe);
        }
        else
        {
            graph::mtl::MaterializationSharedSpec shared_spec;
            if (!graph::mtl::ResolveMaterializationSharedSpec(recipe, materialization_callbacks, shared_spec))
            {
                GLogError("[ResolveMaterialRecipe] ResolveMaterializationSharedSpec failed for recipe=%s", recipe.recipe_name.c_str());
                return false;
            }

            GLogVerbose("[ResolveMaterialRecipe] resolved shared tex=%zu struct=%zu",
                     shared_spec.spec.resources.size(), shared_spec.spec.struct_refs.size());

            MaterializationResolveCacheEntry cache_entry{};
            cache_entry.shared_spec = std::move(shared_spec);
            materialization_resolve_cache[recipe_hash] = std::move(cache_entry);
            out_spec = graph::mtl::MaterializeMaterializationInstance(
                materialization_resolve_cache[recipe_hash].shared_spec, recipe);
        }

        // Rows are instance data. Never return rows retained by the shared cache.
        graph::mtl::MaterializationInstanceData instance_data =
            graph::mtl::MakeMaterializationInstanceData(out_spec);
        uint32_t texture_row = 0;
        uint32_t data_row = 0;
        if (!graph::mtl::WriteMaterializationInstanceToIndexTables(
                out_spec, instance_data, materialization_index_tables, texture_row, data_row))
            return false;

        if (out_texture_layer_row)
            *out_texture_layer_row = texture_row;

        if (out_data_index_row)
            *out_data_index_row = data_row;

        if (out_instance_data)
            *out_instance_data = std::move(instance_data);

        materialization_index_tables_dirty = true;

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
        data_index_rows = static_cast<uint32_t>(materialization_index_tables.GetMaterialDataIndexRowCount());
        return true;
    }

    bool RenderDescriptorBindingSystem::RegisterBindlessTextureResource(const std::string &resource_id, uint32_t bindless_handle)
    {
        if (resource_id.empty() || bindless_handle == 0)
            return false;

        return materialization_texture_pool.RegisterWithHandle(resource_id, bindless_handle) != 0;
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

        return bindless_mgr->Register2DWithResource(materialization_texture_pool, rid, tex, sampler);
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

        return bindless_mgr->Register2DArrayWithResource(materialization_texture_pool, rid, tex, sampler);
    }

    void RenderDescriptorBindingSystem::EnsureMaterializationCallbacks()
    {
        if (!materialization_callbacks.resolve_texture || !materialization_callbacks.resolve_struct)
            materialization_callbacks = graph::mtl::MakePoolResolveCallbacks(materialization_texture_pool, materialization_struct_pool);
    }

    void RenderDescriptorBindingSystem::ReleaseMaterializationIndexBuffers()
    {
        auto *domain_manager = GetResourceDomainManager(context);
        if (domain_manager)
        {
            domain_manager->ClearDomain(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, 0, 0});
            domain_manager->ClearDomain(graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable, 0, 0});

            const uint32_t texture_slot_count = static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE);
            for (uint32_t slot = 1; slot < texture_slot_count; ++slot)
            {
                const uint32_t alias_ssbo_id = graph::mtl::MakeRecipeSSBOId(slot);
                domain_manager->ClearDomain(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, alias_ssbo_id, slot});
            }

            for (uint32_t slot = 1; slot < materialization_data_slot_count; ++slot)
            {
                const uint32_t alias_ssbo_id = graph::mtl::MakeRecipeSSBOId(slot);
                domain_manager->ClearDomain(graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable, alias_ssbo_id, slot});
            }
        }

        materialization_texture_layer_ssbo = nullptr;
        materialization_data_index_table_buffer = nullptr;
        materialization_texture_layer_capacity = 0;
        materialization_data_index_table_capacity = 0;
    }

    void RenderDescriptorBindingSystem::UploadMaterializationIndexTables()
    {
        if (!materialization_index_tables_dirty)
            return;

        auto *domain_manager = GetResourceDomainManager(context);
        if (!domain_manager)
            return;

        const uint32_t texture_rows = static_cast<uint32_t>(materialization_index_tables.GetTextureLayerRowCount());
        const uint32_t data_rows = static_cast<uint32_t>(materialization_index_tables.GetMaterialDataIndexRowCount());

        auto ensure_capacity = [](uint32_t required) -> uint32_t
        {
            if (required == 0)
                return 0;

            uint32_t cap = 1;
            while (cap < required)
                cap <<= 1;
            return cap;
        };

        const uint32_t texture_capacity = ensure_capacity(texture_rows);
        const uint32_t data_capacity = ensure_capacity(data_rows);

        if (texture_capacity > 0)
        {
            const VkDeviceSize byte_size = static_cast<VkDeviceSize>(texture_capacity) * sizeof(graph::mtl::TextureLayerRow);
            materialization_texture_layer_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, 0, 0},
                                                                               "ECS:Materialization:TextureLayerRows",
                                                                               byte_size,
                                                                               texture_capacity,
                                                                               graph::SharingMode::Exclusive);
        }
        else
        {
            materialization_texture_layer_ssbo = domain_manager->GetBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, 0, 0});
        }
        materialization_texture_layer_capacity = domain_manager->GetElementCapacity(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, 0, 0});

        // DataIndex rows use a fixed stride so different material definitions
        // can share the same table buffer.
        const uint32_t max_data_slot_count = materialization_index_tables.GetMaxMaterialDataSlotCount();
        materialization_data_slot_count = max_data_slot_count;
        const VkDeviceSize ssbo_row_stride_bytes = max_data_slot_count > 0
            ? static_cast<VkDeviceSize>(graph::mtl::MaterialDataIndexRowStride) * sizeof(uint32_t)
            : sizeof(uint32_t);  // fallback: at least 1 uint32 to keep buffer valid

        if (data_capacity > 0 && max_data_slot_count > 0)
        {
            const VkDeviceSize byte_size = static_cast<VkDeviceSize>(data_capacity) * ssbo_row_stride_bytes;
            materialization_data_index_table_buffer = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable, 0, 0},
                                                                            "ECS:Materialization:MaterialDataIndexRows",
                                                                            byte_size,
                                                                            data_capacity,
                                                                            graph::SharingMode::Exclusive);
        }
        else
        {
            materialization_data_index_table_buffer = domain_manager->GetBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable, 0, 0});
        }
        materialization_data_index_table_capacity = domain_manager->GetElementCapacity(graph::mtl::SSBOAddress{graph::mtl::SSBOType::MaterialDataIndexTable, 0, 0});

        auto register_domain_aliases = [&](const graph::mtl::SSBOType ssbo_type,
                                           graph::DeviceBuffer *buffer,
                                           const uint32_t element_capacity,
                                           const uint32_t slot_count)
        {
            if (!buffer || element_capacity == 0 || slot_count <= 1)
                return;

            for (uint32_t slot = 1; slot < slot_count; ++slot)
            {
                const uint32_t alias_ssbo_id = graph::mtl::MakeRecipeSSBOId(slot);
                const graph::mtl::SSBOAddress alias_address{ssbo_type, alias_ssbo_id, slot};
                if (!domain_manager->RegisterBuffer(alias_address, buffer, element_capacity))
                {
                    GLogError("[MaterializationAlias] Failed to register alias domain: type=%s ssbo_id=%u slot=%u",
                              graph::mtl::GetSSBOTypeName(ssbo_type),
                              alias_ssbo_id,
                              slot);
                }
            }
        };

        register_domain_aliases(graph::mtl::SSBOType::TextureLayer,
                                materialization_texture_layer_ssbo,
                                materialization_texture_layer_capacity,
                                static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE));
        register_domain_aliases(graph::mtl::SSBOType::MaterialDataIndexTable,
                                materialization_data_index_table_buffer,
                                materialization_data_index_table_capacity,
                                max_data_slot_count);

        if (texture_rows > 0 && materialization_texture_layer_ssbo)
        {
            auto *acc = graph::SSBOArrayAccessor<graph::mtl::TextureLayerRow>::Create(
                materialization_texture_layer_ssbo, texture_rows);
            if (acc)
            {
                for (uint32_t i = 0; i < texture_rows; ++i)
                {
                    const auto *row = materialization_index_tables.GetTextureLayerRow(i);
                    if (!row) break;
                    (*acc)[i] = *row;
                }
                acc->MarkDirty();
                acc->Commit();
                delete acc;
            }
        }

        if (data_rows > 0 && max_data_slot_count > 0 && materialization_data_index_table_buffer)
        {
            // Rows are fixed-width; write as a flat uint32 array.
            auto *acc = graph::SSBOArrayAccessor<uint32_t>::Create(
                materialization_data_index_table_buffer,
                data_rows * graph::mtl::MaterialDataIndexRowStride);
            if (acc)
            {
                for (uint32_t i = 0; i < data_rows; ++i)
                {
                    const auto *row = materialization_index_tables.GetMaterialDataIndexRow(i);
                    if (!row) break;
                    const uint32_t base = i * graph::mtl::MaterialDataIndexRowStride;
                    for (uint32_t j = 0; j < graph::mtl::MaterialDataIndexRowStride; ++j)
                        (*acc)[base + j] = (j < row->values.size()) ? row->values[j] : 0u;
                }
                acc->MarkDirty();
                acc->Commit();
                delete acc;
            }
        }

#ifdef _DEBUG
        if (context)
        {
            if (auto *vk_device = context->GetGPUDevice())
            {
                if (auto *du = vk_device->GetDebugUtils())
                {
                    if (materialization_texture_layer_ssbo)
                        du->SetBuffer(materialization_texture_layer_ssbo->GetBuffer(), "ECS.Materialization.TextureLayerRows");
                    if (materialization_data_index_table_buffer)
                        du->SetBuffer(materialization_data_index_table_buffer->GetBuffer(), "ECS.Materialization.MaterialDataIndexRows");
                }
            }
        }
#endif

        materialization_index_tables_dirty = false;
    }

    const RenderDescriptorBindingSystem::MaterialResourceBinding *RenderDescriptorBindingSystem::FindMaterialResourceBinding(const graph::ShaderProgram *material, const char *name) const
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

        const uint32_t frame_index = context->GetFrameIndex();
        if (materialization_last_reset_frame != frame_index)
        {
            ResetMaterializationFrameData();
            materialization_last_reset_frame = frame_index;
        }

        if (run_contract_diagnostics)
            ValidateResourceLayoutsSideChannel();

        UploadMaterializationIndexTables();

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
                                         const graph::mtl::MaterialResourceRequirement &req,
                                         const char *reason,
                                         int32_t slot = -1)
        {
            if (!material || !req.name || !*req.name)
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
                          req.name,
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
                            req.name,
                            graph::mtl::GetSSBOTypeName(req.ssbo_type),
                            req.ssbo_id,
                            slot,
                            reason ? reason : "unknown");
            }
        };
        auto log_bind_failure = [&](graph::ShaderProgram *material,
                                    MaterialBatch *batch,
                                    const graph::mtl::MaterialResourceRequirement &req,
                                    const char *reason)
        {
            if (!material || !req.name || !*req.name)
                return;

            if (req.required)
            {
                if (batch)
                    batch->descriptor_bind_valid = false;

                GLogError("[DescriptorBinding] Bind failed: material=%s semantic=%s descriptor=%s reason=%s",
                          material->GetName().c_str(),
                          graph::mtl::GetDescriptorSemanticName(req.semantic),
                          req.name,
                          reason ? reason : "unknown");
            }
            else
            {
                GLogWarning("[DescriptorBinding] Optional bind failed: material=%s semantic=%s descriptor=%s reason=%s",
                            material->GetName().c_str(),
                            graph::mtl::GetDescriptorSemanticName(req.semantic),
                            req.name,
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
                            const graph::mtl::MaterialResourceRequirement &req,
                            const graph::IGPUBuffer *gpu) -> bool
        {
            if (!material || !gpu)
                return false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindUBO(req.name, gpu, false);
                return false;
            }

            return material->BindUBO(req.set_type, req.name, gpu, false);
        };

        auto bind_ssbo = [&](graph::ShaderProgram *material,
                             MaterialBatch *batch,
                             const graph::mtl::MaterialResourceRequirement &req,
                             const graph::IGPUBuffer *gpu) -> bool
        {
            if (!material || !gpu)
                return false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindSSBO(req.name, gpu, false);
                return false;
            }

            return material->BindSSBO(req.set_type, req.name, gpu, false);
        };

        auto bind_texture = [&](graph::ShaderProgram *material,
                                MaterialBatch *batch,
                                const graph::mtl::MaterialResourceRequirement &req,
                                graph::Texture *texture) -> bool
        {
            if (!material || !texture)
                return false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindTexture(req.name, texture);
                return false;
            }

            return material->BindTexture(req.set_type, req.name, texture);
        };

        auto bind_texture_sampler = [&](graph::ShaderProgram *material,
                                        MaterialBatch *batch,
                                        const graph::mtl::MaterialResourceRequirement &req,
                                        graph::Texture *texture,
                                        graph::Sampler *sampler) -> bool
        {
            if (!material || !texture || !sampler)
                return false;

            if (batch)
            {
                if (auto *mp = ensure_batch_mp(material, batch, req.set_type))
                    return mp->BindTextureSampler(req.name, texture, sampler);
                return false;
            }

            return material->BindTextureSampler(req.set_type, req.name, texture, sampler);
        };
        auto recipe_runtime_has_layer_table_for_texture_slot = [&](graph::ShaderProgram *material,
                                                                    MaterialBatch *batch,
                                                                    const graph::mtl::MaterialResourceRequirement &req) -> bool
        {
            if (!material || !batch)
                return false;

            const auto &contract = material->GetMaterialResourceLayout();
            for (const auto &candidate : contract.requirements)
            {
                if (candidate.semantic != graph::mtl::DescriptorSemantic::MaterialTextureLayerTable)
                    continue;

                if (candidate.texture_slot != req.texture_slot)
                    continue;

                return true;
            }

            return false;
        };
        auto resolve_recipe_batch_struct_ssbo_id = [&](graph::ShaderProgram *material,
                                                       MaterialBatch *batch,
                                                       const graph::mtl::MaterialResourceRequirement &req,
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
                    if (const auto *resolved = material_comp ? material_comp->FindResolvedSSBOBinding(req.data_slot, req.ssbo_type) : nullptr)
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
                        if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBinding(effective_recipe, req.name, req.ssbo_type))
                        {
                            candidate_ssbo_id = asset->ssbo_id;
                            has_candidate = true;
                        }
                        else if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBindingBySlot(effective_recipe, req.data_slot, req.ssbo_type))
                        {
                            candidate_ssbo_id = asset->ssbo_id;
                            has_candidate = true;
                        }

                        if (has_candidate)
                        {
                            if (auto *entity = primitive_item->GetEntity())
                            {
                                if (auto material_comp = entity->GetComponent<MaterialComponent>())
                                    material_comp->SetResolvedSSBOBinding(req.data_slot, req.ssbo_type, candidate_ssbo_id);
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
                              req.name,
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
                                     const graph::mtl::MaterialResourceRequirement &req)
        {
            switch (req.semantic)
            {
            case graph::mtl::DescriptorSemantic::ViewportInfo:
            {
                if (viewport_ubo)
                {
                    if (!bind_ubo(material, batch, req, viewport_ubo))
                        log_bind_failure(material, batch, req, "bind viewport UBO failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::CameraInfo:
            {
                if (camera_ubo)
                {
                    if (!bind_ubo(material, batch, req, camera_ubo))
                        log_bind_failure(material, batch, req, "bind camera UBO failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::SkyInfo:
            {
                if (sky_ubo)
                {
                    if (!bind_ubo(material, batch, req, sky_ubo))
                        log_bind_failure(material, batch, req, "bind sky UBO failed");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::SkyCubemapSampler:
            {
                const auto *binding = FindMaterialResourceBinding(material, req.name);
                graph::Texture *texture = binding ? binding->texture : nullptr;
                graph::Sampler *sampler = binding ? binding->sampler : nullptr;
                if ((!texture || !sampler) && context)
                {
                    if (auto environment_system = context->GetSystem<EnvironmentSystem>())
                    {
                        texture = environment_system->GetSkyCubeMapTexture();
                        sampler = environment_system->GetSkyCubeMapSampler();
                    }
                }

                if (texture && sampler)
                {
                    if (!bind_texture_sampler(material, batch, req, texture, sampler))
                        log_bind_failure(material, batch, req, "bind sky cubemap sampler failed");
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
                const graph::IGPUBuffer *table_buffer = resolve_domain_ssbo(
                    graph::mtl::SSBOAddress{
                        req.ssbo_type,
                        req.ssbo_id,
                        static_cast<uint32_t>(req.texture_slot)},
                    "MaterialTextureLayerTable");

                if (table_buffer)
                {
                    if (!bind_ssbo(material, batch, req, table_buffer))
                        log_bind_failure(material, batch, req, "bind MaterialTextureLayerTable failed");
                }
                else
                {
                    log_missing_ssbo_once(material, req, "domain binding not found", static_cast<int32_t>(req.texture_slot));
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
            case graph::mtl::DescriptorSemantic::MaterialTexture:
            {
                const bool allow_indirect_fallback = recipe_runtime_has_layer_table_for_texture_slot(material, batch, req);

                const auto *binding = FindMaterialResourceBinding(material, req.name);
                if (binding && binding->texture)
                {
                    const bool bind_ok = binding->sampler
                                       ? bind_texture_sampler(material, batch, req, binding->texture, binding->sampler)
                                       : bind_texture(material, batch, req, binding->texture);
                    if (!bind_ok)
                        log_bind_failure(material, batch, req, binding->sampler ? "bind MaterialTexture(Sampler) failed" : "bind MaterialTexture failed");
                }
                else
                {
                    if (req.required && !allow_indirect_fallback)
                        log_bind_failure(material, batch, req, "missing required MaterialTexture binding");
                }
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialSampler:
            {
                const bool allow_indirect_fallback = recipe_runtime_has_layer_table_for_texture_slot(material, batch, req);

                const auto *binding = FindMaterialResourceBinding(material, req.name);
                if (binding && binding->texture && binding->sampler)
                {
                    if (!bind_texture_sampler(material, batch, req, binding->texture, binding->sampler))
                        log_bind_failure(material, batch, req, "bind MaterialSampler failed");
                }
                else
                {
                    if (req.required && !allow_indirect_fallback)
                        log_bind_failure(material, batch, req, "missing required MaterialSampler binding");
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

            const auto &contract = shader_program->GetMaterialResourceLayout();

            for (const auto &req : contract.requirements)
            {
                if (!req.name || !*req.name)
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

            const auto &contract = shader_program->GetMaterialResourceLayout();

            for (const auto &req : contract.requirements)
            {
                if (!req.name || !*req.name)
                    continue;
                apply_requirement(shader_program, nullptr, req);
            }
        }

        // Bind global bindless descriptor set only for materials that explicitly
        // declare the bindless texture-layer indirection table semantic.
        if (auto *render_context = context->GetRenderContext())
        {
            auto *bindless_mgr = render_context->GetManager<graph::BindlessTextureManager>();
            auto *current_cmd = cmd ? cmd : render_context->GetCurrentRenderCmdBuffer();

            if (bindless_mgr && bindless_mgr->IsValid() && current_cmd)
            {
                constexpr uint32_t bindless_set = static_cast<uint32_t>(graph::DescriptorSetType::Bindless);

                for (const graph::ShaderProgram *material : active_materials)
                {
                    if (!material)
                        continue;

                    bool needs_bindless_set = false;
                    const auto &contract = material->GetMaterialResourceLayout();
                    for (const auto &req : contract.requirements)
                    {
                        if (req.semantic == graph::mtl::DescriptorSemantic::MaterialTextureLayerTable)
                        {
                            needs_bindless_set = true;
                            break;
                        }
                    }
                    if (!needs_bindless_set)
                        continue;

                    const VkPipelineLayout pipeline_layout = material->GetPipelineLayout();
                    if (pipeline_layout == VK_NULL_HANDLE)
                        continue;

                    bindless_mgr->BindToCmd(*current_cmd, pipeline_layout, bindless_set);
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
        case graph::mtl::DescriptorSemantic::SkyCubemapSampler:
            return true;

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

            const auto &contract = shader_program->GetMaterialResourceLayout();

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
