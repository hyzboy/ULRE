#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/UBOCommon.h>
#include<array>
#include<cstdint>
#include<cstring>
#include<unordered_set>
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

    bool RenderDescriptorBindingSystem::BuildMaterialRecipeForMaterial(const graph::Material *material,
                                                                       graph::mtl::MaterialRecipe &out_recipe) const
    {
        out_recipe = {};
        if (!material)
            return false;

        const char *material_name = material->GetName().c_str();
        if (material_name && *material_name)
            out_recipe.recipe_name = material_name;
        else
            out_recipe.recipe_name = "Material@" + std::to_string(reinterpret_cast<uintptr_t>(material));

        out_recipe.shading_model = "Legacy";
        out_recipe.domain = "ECS";

        const auto &contract = material->GetBindingContract();

        constexpr size_t texture_slot_count = static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE);
        std::array<bool, texture_slot_count> used_texture_slots{};
        std::unordered_set<std::string> emitted_texture_keys;

        for (const auto &req : contract.requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialTexture
             && req.semantic != graph::mtl::DescriptorSemantic::MaterialSampler)
                continue;

            if (!req.name || !*req.name)
                continue;

            const std::string key = ToBindingKey(req.name);
            if (key.empty())
                continue;

            if (emitted_texture_keys.find(key) != emitted_texture_keys.end())
                continue;
            emitted_texture_keys.insert(key);

            const MaterialResourceBinding *binding = FindMaterialResourceBinding(material, req.name);
            std::string resource_id = binding ? BuildTextureResourceId(binding->texture) : std::string();

            if (resource_id.empty() && !req.required)
                continue;

            const graph::mtl::TextureSlot slot = req.texture_slot;

            const size_t slot_index = static_cast<size_t>(slot);
            if (slot_index >= used_texture_slots.size() || used_texture_slots[slot_index])
                continue;
            used_texture_slots[slot_index] = true;

            graph::mtl::RecipeTextureBinding texture_binding{};
            texture_binding.slot = slot;
            texture_binding.resource_id = std::move(resource_id);
            texture_binding.required = req.required;
            out_recipe.textures.emplace_back(std::move(texture_binding));
        }

        std::unordered_set<uint64_t> emitted_struct_keys;
        for (const auto &req : contract.requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            const uint64_t struct_key =
                (static_cast<uint64_t>(req.ssbo_type) << 32) | static_cast<uint64_t>(req.data_slot);
            if (emitted_struct_keys.find(struct_key) != emitted_struct_keys.end())
                continue;
            emitted_struct_keys.insert(struct_key);

            graph::mtl::RecipeStructBinding struct_binding{};
            struct_binding.slot = req.data_slot;
            struct_binding.ssbo_type = req.ssbo_type;
            struct_binding.ssbo_id = graph::mtl::MakeRecipeSSBOId(0);
            struct_binding.shared_across_instances = false;
            out_recipe.structs.emplace_back(std::move(struct_binding));
        }

        return true;
    }

    bool RenderDescriptorBindingSystem::BuildMaterialRecipeForRenderItem(const RenderItem *item,
                                                                         graph::mtl::MaterialRecipe &out_recipe) const
    {
        out_recipe = {};
        if (!item)
            return false;

        graph::Material *material = item->GetMaterial();
        if (!BuildMaterialRecipeForMaterial(material, out_recipe))
            return false;

        graph::MaterialInstance *mi = item->GetMaterialInstance();
        if (mi)
        {
            if (!out_recipe.domain.empty())
                out_recipe.domain += "|";
            out_recipe.domain += "mi:";
            out_recipe.domain += std::to_string(mi->GetMIID());
        }

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

    bool RenderDescriptorBindingSystem::RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
                                                                     uint32_t ssbo_id,
                                                                     uint32_t byte_stride)
    {
        if (auto *domain_manager = GetResourceDomainManager(context))
            domain_manager->Touch(graph::mtl::SSBOAddress{ssbo_type, ssbo_id, 0});

        return materialization_struct_pool.RegisterLayout(ssbo_type, ssbo_id, byte_stride);
    }

    void RenderDescriptorBindingSystem::ResetMaterializationFrameData()
    {
        materialization_struct_pool.ResetAllocations();
        materialization_index_tables.Clear();
        materialization_resolve_cache.clear();
        materialization_index_tables_dirty = true;
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

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(recipe);
        auto cache_it = materialization_resolve_cache.find(recipe_hash);
        if (cache_it != materialization_resolve_cache.end())
        {
            out_spec = cache_it->second.spec;

            if (out_texture_layer_row)
                *out_texture_layer_row = cache_it->second.texture_layer_row;

            if (out_data_index_row)
                *out_data_index_row = cache_it->second.data_index_row;

            return true;
        }

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

        MaterializationResolveCacheEntry cache_entry{};
        cache_entry.spec = out_spec;
        cache_entry.texture_layer_row = texture_row;
        cache_entry.data_index_row = data_row;
        materialization_resolve_cache[recipe_hash] = std::move(cache_entry);
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
        data_index_rows = static_cast<uint32_t>(materialization_index_tables.GetDataIndexRowCount());
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
            domain_manager->ClearDomain(graph::mtl::SSBOAddress{graph::mtl::SSBOType::DataIndex, 0, 0});
        }

        materialization_texture_layer_ssbo = nullptr;
        materialization_data_index_ssbo = nullptr;
        materialization_texture_layer_capacity = 0;
        materialization_data_index_capacity = 0;
    }

    void RenderDescriptorBindingSystem::UploadMaterializationIndexTables()
    {
        if (!materialization_index_tables_dirty)
            return;

        auto *domain_manager = GetResourceDomainManager(context);
        if (!domain_manager)
            return;

        const uint32_t texture_rows = static_cast<uint32_t>(materialization_index_tables.GetTextureLayerRowCount());
        const uint32_t data_rows = static_cast<uint32_t>(materialization_index_tables.GetDataIndexRowCount());

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

        if (data_capacity > 0)
        {
            const VkDeviceSize byte_size = static_cast<VkDeviceSize>(data_capacity) * sizeof(graph::mtl::DataIndexRow);
            materialization_data_index_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::DataIndex, 0, 0},
                                                                            "ECS:Materialization:DataIndexRows",
                                                                            byte_size,
                                                                            data_capacity,
                                                                            graph::SharingMode::Exclusive);
        }
        else
        {
            materialization_data_index_ssbo = domain_manager->GetBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::DataIndex, 0, 0});
        }
        materialization_data_index_capacity = domain_manager->GetElementCapacity(graph::mtl::SSBOAddress{graph::mtl::SSBOType::DataIndex, 0, 0});

        if (texture_rows > 0 && materialization_texture_layer_ssbo && materialization_texture_layer_ssbo->GetGPUBuffer())
        {
            auto *gpu = materialization_texture_layer_ssbo->GetGPUBuffer();
            auto *dst = static_cast<std::uint8_t *>(gpu->Map(0, static_cast<VkDeviceSize>(texture_rows) * sizeof(graph::mtl::TextureLayerRow)));
            if (dst)
            {
                for (uint32_t i = 0; i < texture_rows; ++i)
                {
                    const auto *row = materialization_index_tables.GetTextureLayerRow(i);
                    if (!row)
                        break;

                    std::memcpy(dst + static_cast<size_t>(i) * sizeof(graph::mtl::TextureLayerRow),
                                row,
                                sizeof(graph::mtl::TextureLayerRow));
                }
                gpu->Unmap();
            }
        }

        if (data_rows > 0 && materialization_data_index_ssbo && materialization_data_index_ssbo->GetGPUBuffer())
        {
            auto *gpu = materialization_data_index_ssbo->GetGPUBuffer();
            auto *dst = static_cast<std::uint8_t *>(gpu->Map(0, static_cast<VkDeviceSize>(data_rows) * sizeof(graph::mtl::DataIndexRow)));
            if (dst)
            {
                for (uint32_t i = 0; i < data_rows; ++i)
                {
                    const auto *row = materialization_index_tables.GetDataIndexRow(i);
                    if (!row)
                        break;

                    std::memcpy(dst + static_cast<size_t>(i) * sizeof(graph::mtl::DataIndexRow),
                                row,
                                sizeof(graph::mtl::DataIndexRow));
                }
                gpu->Unmap();
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
                    if (materialization_data_index_ssbo)
                        du->SetBuffer(materialization_data_index_ssbo->GetBuffer(), "ECS.Materialization.DataIndexRows");
                }
            }
        }
#endif

        materialization_index_tables_dirty = false;
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
            ValidateContractsSideChannel();

        UploadMaterializationIndexTables();

        EnsureViewportUBO();

        ApplyContractBindings(cmd);
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

    void RenderDescriptorBindingSystem::ApplyContractBindings(graph::RenderCmdBuffer *cmd)
    {
        if (!context)
            return;

        const auto *viewport_ubo = ResolveViewportUBO();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();
        auto *domain_manager = GetResourceDomainManager(context);

        const graph::IGPUBuffer *texture_layer_table_buffer = nullptr;
        if (domain_manager)
            texture_layer_table_buffer = domain_manager->GetGPUBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, 0, 0});

        const graph::IGPUBuffer *data_index_table_buffer = nullptr;
        if (domain_manager)
            data_index_table_buffer = domain_manager->GetGPUBuffer(graph::mtl::SSBOAddress{graph::mtl::SSBOType::DataIndex, 0, 0});

        const auto &cache = context->GetRenderFrameCache();

        std::unordered_set<graph::Material *> l2w_bound_materials;
        std::unordered_set<graph::Material *> mi_bound_materials;
        std::unordered_set<const graph::Material *> active_materials;

        auto apply_requirement = [&](graph::Material *material,
                                     const MaterialBatch *batch,
                                     const graph::mtl::DescriptorRequirement &req)
        {
            switch (req.semantic)
            {
            case graph::mtl::DescriptorSemantic::ViewportInfo:
            {
                if (viewport_ubo)
                    material->BindUBO(req.set_type, req.name, viewport_ubo, false);
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
            case graph::mtl::DescriptorSemantic::SkyCubemapSampler:
            {
                const auto *binding = FindMaterialResourceBinding(material, req.name);
                if (binding && binding->texture && binding->sampler)
                    material->BindTextureSampler(req.set_type, req.name, binding->texture, binding->sampler);
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
            case graph::mtl::DescriptorSemantic::LocalToWorldIndexTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;

                // Prefer the per-batch buffer written in draw order by PrimitiveBatchPipeline.
                if (batch && batch->l2w_index_rows_buffer)
                {
                    table_buffer = batch->l2w_index_rows_buffer->GetGPUBuffer();
                }

                // Fallback: global TransformIndexRows from TransformAssignmentBuffer.
                if (!table_buffer && batch && batch->transform_buffer)
                {
                    auto *rows_buffer = batch->transform_buffer->GetTransformIndexRowsBuffer();
                    table_buffer = rows_buffer ? rows_buffer->GetGPUBuffer() : nullptr;
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
                    table_buffer = domain_manager->GetGPUBuffer(
                        graph::mtl::SSBOAddress{
                            graph::mtl::SSBOType::TransformIndexRows,
                            graph::mtl::ECSReservedSSBOId::TransformIndexRows,
                            0});
                }

                if (table_buffer)
                    material->BindSSBO(req.set_type, req.name, table_buffer, false);
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
            case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;
                if (batch && batch->mi_buffer)
                {
                    auto *rows_buffer = batch->mi_buffer->GetTextureLayerRowsBuffer();
                    table_buffer = rows_buffer ? rows_buffer->GetGPUBuffer() : nullptr;
                }

                if (!table_buffer)
                    table_buffer = texture_layer_table_buffer;

                if (table_buffer)
                    material->BindSSBO(req.set_type, req.name, table_buffer, false);
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
            {
                const graph::IGPUBuffer *table_buffer = nullptr;
                if (batch && batch->mi_buffer)
                {
                    auto *rows_buffer = batch->mi_buffer->GetDataIndexRowsBuffer();
                    table_buffer = rows_buffer ? rows_buffer->GetGPUBuffer() : nullptr;
                }

                if (!table_buffer)
                    table_buffer = data_index_table_buffer;

                if (table_buffer)
                    material->BindSSBO(req.set_type, req.name, table_buffer, false);
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
            case graph::mtl::DescriptorSemantic::MaterialColorPalette:
            {
                // MaterialColorPalette is explicitly declared and validated at compile time.
                // Runtime binding is currently handled by non-ECS material setup paths.
                break;
            }
            default:
                break;
            }
        };

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
                apply_requirement(material, batch, req);
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
                apply_requirement(material, nullptr, req);
            }
        }

        // Bind global bindless descriptor set only for materials that explicitly
        // declare the bindless texture-layer indirection table semantic.
        if (auto *render_context = context->GetRenderContext())
        {
            auto *bindless_mgr = render_context->GetBindlessTextureManager();
            auto *current_cmd = cmd ? cmd : render_context->GetCurrentRenderCmdBuffer();

            if (bindless_mgr && bindless_mgr->IsValid() && current_cmd)
            {
                constexpr uint32_t bindless_set = static_cast<uint32_t>(graph::DescriptorSetType::Bindless);

                for (const graph::Material *material : active_materials)
                {
                    if (!material)
                        continue;

                    bool needs_bindless_set = false;
                    const auto &contract = material->GetBindingContract();
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
        case graph::mtl::DescriptorSemantic::SkyCubemapSampler:
            return true;

        case graph::mtl::DescriptorSemantic::LocalToWorld:
        case graph::mtl::DescriptorSemantic::LocalToWorldIndexTable:
        case graph::mtl::DescriptorSemantic::MaterialColorPalette:
        case graph::mtl::DescriptorSemantic::MaterialInstance:
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
