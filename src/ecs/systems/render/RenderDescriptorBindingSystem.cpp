#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/log/Log.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/UBOCommon.h>
#include<array>
#include<algorithm>
#include<cctype>
#include<cstdint>
#include<cstring>
#include<unordered_set>
#include<string>

namespace hgl::ecs
{
    namespace
    {
        // P1.5-v1-01: scope lock (MI-only materialization).
        // Keep MaterialRecipe resolve path focused on MaterialInstance data only.
        // Texture materialization (including array-specific rows) is deferred to P1.6.
        constexpr bool kP15V1ScopeLockMIOnly = true;

        std::string ToBindingKey(const char *name)
        {
            return name ? std::string(name) : std::string();
        }

        std::string ToBindingKey(const AnsiString &name)
        {
            return ToBindingKey(name.c_str());
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool ContainsToken(const std::string &haystack, const char *needle)
        {
            return needle && *needle && haystack.find(needle) != std::string::npos;
        }

        graph::mtl::TextureSlot InferTextureSlotFromBindingName(const std::string &binding_name_lower, bool &out_known)
        {
            out_known = true;

            if (ContainsToken(binding_name_lower, "normal"))
                return graph::mtl::TextureSlot::Normal;
            if (ContainsToken(binding_name_lower, "metallic") || ContainsToken(binding_name_lower, "metalness"))
                return graph::mtl::TextureSlot::Metallic;
            if (ContainsToken(binding_name_lower, "roughness") || ContainsToken(binding_name_lower, "rough"))
                return graph::mtl::TextureSlot::Roughness;
            if (ContainsToken(binding_name_lower, "emissive") || ContainsToken(binding_name_lower, "emission"))
                return graph::mtl::TextureSlot::Emissive;
            if (ContainsToken(binding_name_lower, "occlusion") || ContainsToken(binding_name_lower, "ambientocclusion") || ContainsToken(binding_name_lower, "ao"))
                return graph::mtl::TextureSlot::Occlusion;
            if (ContainsToken(binding_name_lower, "opacity") || ContainsToken(binding_name_lower, "alphamask") || ContainsToken(binding_name_lower, "alpha") || ContainsToken(binding_name_lower, "mask"))
                return graph::mtl::TextureSlot::OpacityMask;
            if (ContainsToken(binding_name_lower, "height") || ContainsToken(binding_name_lower, "parallax") || ContainsToken(binding_name_lower, "displacement"))
                return graph::mtl::TextureSlot::Height;
            if (ContainsToken(binding_name_lower, "basecolor") || ContainsToken(binding_name_lower, "albedo") || ContainsToken(binding_name_lower, "diffuse") || ContainsToken(binding_name_lower, "color"))
                return graph::mtl::TextureSlot::BaseColor;

            out_known = false;
            return graph::mtl::TextureSlot::Custom0;
        }

        graph::mtl::DataSlot InferDataSlotFromStructName(const char *struct_name)
        {
            const std::string lower_name = ToLowerAscii(ToBindingKey(struct_name));

            if (ContainsToken(lower_name, "emissive"))
                return graph::mtl::DataSlot::EmissiveSurface;
            if (ContainsToken(lower_name, "clearcoat"))
                return graph::mtl::DataSlot::ClearCoatSurface;
            if (ContainsToken(lower_name, "transmission") || ContainsToken(lower_name, "refract"))
                return graph::mtl::DataSlot::TransmissionSurface;

            return graph::mtl::DataSlot::PBRSurface;
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

        if (!kP15V1ScopeLockMIOnly)
        {
            constexpr size_t texture_slot_count = static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE);
            std::array<bool, texture_slot_count> used_texture_slots{};
            std::unordered_set<std::string> emitted_texture_keys;
            uint32_t custom_slot_cursor = 0;

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

                const std::string key_lower = ToLowerAscii(key);
                bool known_slot = false;
                graph::mtl::TextureSlot slot = InferTextureSlotFromBindingName(key_lower, known_slot);

                if (!known_slot)
                {
                    if (custom_slot_cursor == 0)
                        slot = graph::mtl::TextureSlot::Custom0;
                    else if (custom_slot_cursor == 1)
                        slot = graph::mtl::TextureSlot::Custom1;
                    else
                        continue;

                    ++custom_slot_cursor;
                }

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
        }

        std::unordered_set<std::string> emitted_struct_names;
        for (const auto &req : contract.requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            if (!req.struct_name || !*req.struct_name)
                continue;

            const std::string struct_name = ToBindingKey(req.struct_name);
            if (struct_name.empty())
                continue;
            if (emitted_struct_names.find(struct_name) != emitted_struct_names.end())
                continue;
            emitted_struct_names.insert(struct_name);

            graph::mtl::RecipeStructBinding struct_binding{};
            struct_binding.slot = InferDataSlotFromStructName(req.struct_name);
            struct_binding.ssbo_type = graph::mtl::DefaultSSBOTypeForDataSlot(struct_binding.slot);
            struct_binding.resource_domain_id = 0;
            struct_binding.struct_name = struct_name;
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
                                                                     uint32_t resource_domain_id,
                                                                     uint32_t byte_stride,
                                                                     const std::string &struct_name)
    {
        if (auto *domain_manager = GetResourceDomainManager(context))
            domain_manager->Touch(ssbo_type, resource_domain_id);

        return materialization_struct_pool.RegisterLayout(ssbo_type, resource_domain_id, byte_stride, struct_name);
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
        if (kP15V1ScopeLockMIOnly)
        {
            const graph::mtl::DataIndexRow data_index_row = graph::mtl::BuildDataIndexRow(out_spec);
            data_row = materialization_index_tables.PushDataIndexRow(data_index_row);
            texture_row = 0;
        }
        else
        {
            if (!graph::mtl::WriteSpecToIndexTables(out_spec, materialization_index_tables, texture_row, data_row))
                return false;
        }

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
            domain_manager->ClearDomain(graph::mtl::SSBOType::TextureLayer, 0);
            domain_manager->ClearDomain(graph::mtl::SSBOType::DataIndex, 0);
        }
        else
        {
            auto *bm = GetBufferManager(context);
            if (bm)
            {
                if (materialization_texture_layer_ssbo)
                    bm->Release(materialization_texture_layer_ssbo);
                if (materialization_data_index_ssbo)
                    bm->Release(materialization_data_index_ssbo);
            }
            else
            {
                delete materialization_texture_layer_ssbo;
                delete materialization_data_index_ssbo;
            }
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

        auto *bm = GetBufferManager(context);
        if (!bm)
            return;

        const uint32_t texture_rows = kP15V1ScopeLockMIOnly
                                    ? 0
                                    : static_cast<uint32_t>(materialization_index_tables.GetTextureLayerRowCount());
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

        auto *domain_manager = GetResourceDomainManager(context);

        if (domain_manager)
        {
            if (texture_capacity > 0)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(texture_capacity) * sizeof(graph::mtl::TextureLayerRow);
                materialization_texture_layer_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOType::TextureLayer,
                                                                                  0,
                                                                                  "ECS:Materialization:TextureLayerRows",
                                                                                  byte_size,
                                                                                  texture_capacity,
                                                                                  graph::SharingMode::Exclusive);
            }
            else
            {
                materialization_texture_layer_ssbo = domain_manager->GetBuffer(graph::mtl::SSBOType::TextureLayer, 0);
            }

            materialization_texture_layer_capacity = domain_manager->GetElementCapacity(graph::mtl::SSBOType::TextureLayer, 0);
        }
        else if (texture_capacity > materialization_texture_layer_capacity)
        {
            if (materialization_texture_layer_ssbo)
                bm->Release(materialization_texture_layer_ssbo);

            const VkDeviceSize byte_size = static_cast<VkDeviceSize>(texture_capacity) * sizeof(graph::mtl::TextureLayerRow);
            materialization_texture_layer_ssbo = bm->CreateSSBO("ECS:Materialization:TextureLayerRows", byte_size, graph::SharingMode::Exclusive);
            materialization_texture_layer_capacity = materialization_texture_layer_ssbo ? texture_capacity : 0;
        }

        if (domain_manager)
        {
            if (data_capacity > 0)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(data_capacity) * sizeof(graph::mtl::DataIndexRow);
                materialization_data_index_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOType::DataIndex,
                                                                               0,
                                                                               "ECS:Materialization:DataIndexRows",
                                                                               byte_size,
                                                                               data_capacity,
                                                                               graph::SharingMode::Exclusive);
            }
            else
            {
                materialization_data_index_ssbo = domain_manager->GetBuffer(graph::mtl::SSBOType::DataIndex, 0);
            }

            materialization_data_index_capacity = domain_manager->GetElementCapacity(graph::mtl::SSBOType::DataIndex, 0);
        }
        else if (data_capacity > materialization_data_index_capacity)
        {
            if (materialization_data_index_ssbo)
                bm->Release(materialization_data_index_ssbo);

            const VkDeviceSize byte_size = static_cast<VkDeviceSize>(data_capacity) * sizeof(graph::mtl::DataIndexRow);
            materialization_data_index_ssbo = bm->CreateSSBO("ECS:Materialization:DataIndexRows", byte_size, graph::SharingMode::Exclusive);
            materialization_data_index_capacity = materialization_data_index_ssbo ? data_capacity : 0;
        }

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

        UploadMaterializationIndexTables();

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

        const auto *viewport_ubo = ResolveViewportUBO();
        const auto *camera_ubo = ResolveCameraUBO();
        const auto *sky_ubo = ResolveSkyUBO();
        auto *domain_manager = GetResourceDomainManager(context);

        const graph::IGPUBuffer *texture_layer_table_buffer = nullptr;
        if (!kP15V1ScopeLockMIOnly)
        {
            if (domain_manager)
                texture_layer_table_buffer = domain_manager->GetGPUBuffer(graph::mtl::SSBOType::TextureLayer, 0);
            else if (materialization_texture_layer_ssbo)
                texture_layer_table_buffer = materialization_texture_layer_ssbo->GetGPUBuffer();
        }

        const graph::IGPUBuffer *data_index_table_buffer = nullptr;
        if (domain_manager)
            data_index_table_buffer = domain_manager->GetGPUBuffer(graph::mtl::SSBOType::DataIndex, 0);
        else if (materialization_data_index_ssbo)
            data_index_table_buffer = materialization_data_index_ssbo->GetGPUBuffer();

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
            case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
            {
                if (kP15V1ScopeLockMIOnly)
                    break;

                if (texture_layer_table_buffer)
                    material->BindSSBO(req.set_type, req.name, texture_layer_table_buffer, false);
                break;
            }
            case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
            {
                if (data_index_table_buffer)
                    material->BindSSBO(req.set_type, req.name, data_index_table_buffer, false);
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
            return true;
        case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
            if (kP15V1ScopeLockMIOnly)
                return false;
            if (auto *domain_manager = GetResourceDomainManager(context))
                return domain_manager->GetGPUBuffer(graph::mtl::SSBOType::TextureLayer, 0) != nullptr;
            return materialization_texture_layer_ssbo && materialization_texture_layer_ssbo->GetGPUBuffer();
        case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
            if (auto *domain_manager = GetResourceDomainManager(context))
                return domain_manager->GetGPUBuffer(graph::mtl::SSBOType::DataIndex, 0) != nullptr;
            return materialization_data_index_ssbo && materialization_data_index_ssbo->GetGPUBuffer();
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
