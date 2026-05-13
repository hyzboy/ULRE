#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>  // execution-order dependency only; no Quad API called
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/graph/module/TextureDomainRegistry.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/mtl/MaterialRecipeStore.h>
#include<hgl/log/Log.h>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKDomainResourceBinding.h>

namespace hgl::ecs
{
    namespace
    {
        static uint64_t g_runtime_texture_binding_id = 1;

        static bool PatchTextureSlotRecipe(graph::mtl::MaterialRecipe &recipe,
                                           graph::mtl::SamplerSlot slot,
                                           graph::mtl::TextureSourceMode source_mode,
                                           const std::string &path)
        {
            for (auto &tc : recipe.textures)
            {
                if (tc.slot != slot)
                    continue;

                tc.source_mode = source_mode;
                tc.path = path;
                return true;
            }

            recipe.textures.push_back({ slot, source_mode, path });
            return true;
        }

        static bool BuildBoundRecipe(ECSContext *world,
                                     PrimitiveComponent *primitive,
                                     const TextureBindingTask &binding,
                                     graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!world || !primitive)
                return false;

            auto *graphics_context = world->GetGraphicsContext();
            auto *recipe_store = graphics_context ? graphics_context->GetRecipeStore() : nullptr;
            if (!recipe_store)
                return false;

            const auto &resolve_request = primitive->GetMaterialResolveRequest();
            const auto *base_recipe = recipe_store->GetRecipe(resolve_request.recipe_id);
            if (!base_recipe)
                return false;

            out_recipe = *base_recipe;
            out_recipe.domain_id = binding.HasDomainTag() ? binding.domain_tag : std::string();

            return PatchTextureSlotRecipe(out_recipe,
                                          binding.slot,
                                          binding.HasDomainTag() ? graph::mtl::TextureSourceMode::Array
                                                                 : binding.source_mode,
                                          binding.HasDomainTag() ? std::string()
                                                                 : ToStdString(binding.texture_path));
        }

        static void UpdateResolvedMaterialState(PrimitiveComponent *primitive,
                                                graph::MaterialBindingInstance *mi,
                                                graph::ShaderMaterialProgram *material,
                                                const graph::VIL *resolved_vil)
        {
            if (!primitive || !mi)
                return;

            auto &request = primitive->GetMaterialResolveRequest();
            request.resolved_binding_instance = mi;
            request.resolved_material = material;
            request.resolved_domain = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
            request.resolved_domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);
            request.resolved_vil = resolved_vil ? resolved_vil : primitive->GetResolvedVIL();
            request.resolved_mi_id = mi->GetMIID();
            request.resolved_preset = mi->GetRenderPreset();
            request.dirty = false;
        }

        static PrimitiveComponent::ResolvedMaterialState BuildStagingMaterialState(
            PrimitiveComponent *primitive,
            graph::MaterialBindingInstance *mi,
            graph::ShaderMaterialProgram *material,
            const graph::VIL *resolved_vil,
            const RuntimeTextureBinding &runtime_binding)
        {
            PrimitiveComponent::ResolvedMaterialState state{};
            state.binding_instance = mi;
            state.material = material;
            state.domain = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
            state.domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);
            state.runtime_texture_binding = runtime_binding;
            state.vil = resolved_vil ? resolved_vil : (primitive ? primitive->GetResolvedVIL() : nullptr);
            state.mi_id = mi ? mi->GetMIID() : -1;
            state.preset = mi ? mi->GetRenderPreset() : hgl::graph::GraphicsPipelinePreset::Solid3D;

            if (state.runtime_texture_binding.IsReady() && state.runtime_texture_binding.domain)
                state.domain = state.runtime_texture_binding.domain;

            return state;
        }

        static bool EnsurePrimitiveBindingInstance(graph::PrimitiveManager *primitive_manager,
                                                   PrimitiveComponent *primitive,
                                                   graph::MaterialBindingInstance *mi,
                                                   const graph::VIL *resolved_vil)
        {
            if (!primitive_manager || !primitive || !mi)
                return false;

            graph::Primitive *current_primitive = primitive->GetPrimitive();
            graph::Geometry *geometry = current_primitive ? current_primitive->GetGeometry()
                                                          : primitive->GetUnresolvedGeometry();
            if (!geometry)
                return false;

            if (!current_primitive)
            {
                auto *new_primitive = primitive_manager->CreatePrimitive(geometry, mi, resolved_vil);
                if (!new_primitive)
                    return false;

                primitive->SetPrimitive(new_primitive);
                primitive->SetUnresolvedGeometry(nullptr);
                return true;
            }

            if (current_primitive->ChangeMaterialInstance(mi))
                return true;

            auto *replacement = primitive_manager->CreatePrimitive(geometry, mi, resolved_vil);
            if (!replacement)
                return false;

            primitive->SetPrimitive(replacement);
            primitive_manager->Release(current_primitive);
            return true;
        }
    }

    TextureMaterialBindingSystem::TextureMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Primitive");
        // Execution-order dependency only: ensures domain texture arrays are rebuilt before this system runs.
        // TODO(Phase 5): replace with a generic DomainResourcesReadySystem.
        AddDependency<QuadResourcePrepareSystem>();
        AddDependency<MaterialResolveSystem>();
    }

    void TextureMaterialBindingSystem::SubmitTextureBindingTask(const TextureBindingTask &task)
    {
        if (!task.entity_id.IsValid() || task.texture_path.IsEmpty())
            return;

        pending_tasks.push_back(task);
    }

    bool TextureMaterialBindingSystem::SubmitTextureBindingRequest(EntityID entity_id,
                                                                   const hgl::OSString &texture_path,
                                                                   const std::string &domain_tag,
                                                                   graph::mtl::SamplerSlot slot,
                                                                   graph::mtl::TextureSourceMode source_mode,
                                                                   graph::TextureChannelHint channel_hint)
    {
        if (!entity_id.IsValid() || texture_path.IsEmpty())
            return false;

        TextureBindingTask task{};
        task.entity_id = entity_id;
        task.texture_path = texture_path;
        task.domain_tag = domain_tag;
        task.slot = slot;
        task.source_mode = source_mode;
        task.channel_hint = channel_hint;
        SubmitTextureBindingTask(task);
        return true;
    }

    void TextureMaterialBindingSystem::Update(float)
    {
        if (!world)
            return;

        if (pending_tasks.empty())
            return;

        std::vector<TextureBindingTask> retry_tasks;
        retry_tasks.reserve(pending_tasks.size());

        for (const auto &task : pending_tasks)
        {
            Entity *entity = world->GetEntity(task.entity_id);
            if (!entity)
                continue;

            auto primitive = entity->GetComponent<PrimitiveComponent>();
            if (!primitive || !primitive->IsVisible())
                continue;

            if (!EnsurePrimitiveTextureBinding(primitive.get(), task))
                retry_tasks.push_back(task);
        }

        pending_tasks.swap(retry_tasks);
    }

    bool TextureMaterialBindingSystem::EnsurePrimitiveTextureBinding(PrimitiveComponent *primitive,
                                                                     const TextureBindingTask &binding)
    {
        if (!world || !primitive)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] missing input: world=%p primitive=%p binding=%p",
                    static_cast<void *>(world),
                    static_cast<void *>(primitive),
                    nullptr);
            return false;
        }

        const auto &texture_path = binding.texture_path;
        if (texture_path.IsEmpty())
            return true;

        auto *graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] graphics_context is null: primitive=%p", static_cast<void *>(primitive));
            return false;
        }

        auto *texture_manager = graphics_context->GetTextureManager();
        auto *sampler_manager = graphics_context->GetSamplerManager();
        auto *primitive_manager = graphics_context->GetPrimitiveManager();
        auto *recipe_registry = graphics_context->GetMaterialAssetRegistry();
        if (!texture_manager || !sampler_manager || !primitive_manager || !recipe_registry)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] missing managers: texture_manager=%p sampler_manager=%p primitive_manager=%p recipe_registry=%p",
                    static_cast<void *>(texture_manager),
                    static_cast<void *>(sampler_manager),
                    static_cast<void *>(primitive_manager),
                    static_cast<void *>(recipe_registry));
            return false;
        }

        graph::Geometry *geometry = primitive->GetPrimitive() ? primitive->GetPrimitive()->GetGeometry()
                                                              : primitive->GetUnresolvedGeometry();
        if (!geometry)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] geometry is null: primitive=%p prim=%p unresolved_geom=%p",
                    static_cast<void *>(primitive),
                    static_cast<void *>(primitive->GetPrimitive()),
                    static_cast<void *>(primitive->GetUnresolvedGeometry()));
            return false;
        }

        if (!binding.HasDomainTag())
        {
            auto *previous_material = primitive->GetShaderMaterialProgram();
            auto *descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>().get();
            const auto resolved_state = primitive->ResolveEffectiveMaterialState();
            auto *mi = resolved_state.binding_instance;
            auto *material = resolved_state.material;
            auto *mi_binding = mi ? graph::MaterialBindingInstanceInternalAccess::GetDomainBinding(mi) : nullptr;
            const graph::VIL *resolved_vil = resolved_state.vil;

            if (!mi || !material)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] Non-domain: missing resolved state: primitive=%p mi=%p material=%p texture='%s'",
                        static_cast<void *>(primitive),
                        static_cast<void *>(mi),
                        static_cast<void *>(material),
                        texture_path.c_str());
                return false;
            }

            LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain: reusing resolved state: primitive=%p mi=%p material=%p(%s) mi.binding=%p slot=%u",
                    static_cast<void *>(primitive),
                    static_cast<void *>(mi),
                    static_cast<void *>(material),
                    material->GetName().c_str(),
                    static_cast<void *>(mi_binding),
                    static_cast<unsigned>(binding.slot));
            LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain request source: texture='%s' source_mode=%u",
                    texture_path.c_str(),
                    static_cast<unsigned>(binding.source_mode));

            auto *texture = texture_manager->LoadTexture2D(texture_path, true);
            if (!texture)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] LoadTexture2D failed: texture='%s'", texture_path.c_str());
                return false;
            }

            auto *sampler = QuadResourcePrepareSystem::GetSharedSampler();
            if (!sampler)
                sampler = sampler_manager->CreateSampler(texture);
            if (!sampler)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] sampler is null: texture='%s'",
                        texture_path.c_str());
                return false;
            }

            if (mi_binding)
            {
                mi_binding->BindResourceSampler(binding.slot, texture, sampler);
                mi_binding->Update();
                auto *binding_pm = mi_binding->GetPerMaterialMP();
                LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain mi.binding updated: binding=%p pm=%p ds=%p tex=%p sampler=%p",
                        static_cast<void *>(mi_binding),
                        static_cast<void *>(binding_pm),
                        binding_pm ? (void *)binding_pm->GetVkDescriptorSet() : nullptr,
                        static_cast<void *>(texture),
                        static_cast<void *>(sampler));
            }

            if (!material->BindResourceSampler(binding.slot, texture, sampler))
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] material->BindResourceSampler failed (non-domain): material=%p(%s) slot=%u tex=%p sampler=%p",
                        static_cast<void *>(material),
                        material->GetName().c_str(),
                        static_cast<unsigned>(binding.slot),
                        static_cast<void *>(texture),
                        static_cast<void *>(sampler));
                return false;
            }
            material->Update();
            auto *material_pm = material->GetMP(hgl::graph::DescriptorSetType::PerMaterial);
            LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain material updated: material=%p(%s) pm=%p ds=%p tex=%p sampler=%p",
                    static_cast<void *>(material),
                    material->GetName().c_str(),
                    static_cast<void *>(material_pm),
                    material_pm ? (void *)material_pm->GetVkDescriptorSet() : nullptr,
                    static_cast<void *>(texture),
                    static_cast<void *>(sampler));

            if (descriptor_binding_system)
            {
                if (previous_material && previous_material != material)
                    descriptor_binding_system->ClearMaterialBindings(previous_material);

                if (mi_binding)
                {
                    descriptor_binding_system->RegisterDomainBinding(mi_binding);
                    descriptor_binding_system->RegisterDomainTextureSampler(mi_binding,
                                                                           binding.slot,
                                                                           texture,
                                                                           sampler);
                }

                descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                          binding.slot,
                                                                          texture,
                                                                          sampler);
            }

            math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
            mi->WriteMIData(texture_size);

            RuntimeTextureBinding runtime_binding{};
            runtime_binding.kind = RuntimeTextureBinding::Kind::SingleTexture;
            runtime_binding.texture = texture;
            runtime_binding.sampler = sampler;
            runtime_binding.domain_binding = mi_binding;
            runtime_binding.domain = resolved_state.domain;
            runtime_binding.binding_id = g_runtime_texture_binding_id++;
            runtime_binding.status = RuntimeTextureBinding::Status::Ready;
            runtime_binding.ready = true;
            primitive->SetRuntimeTextureBinding(runtime_binding);

            UpdateResolvedMaterialState(primitive, mi, material, resolved_vil);
            primitive->SetStagingRenderState(BuildStagingMaterialState(primitive,
                                                                       mi,
                                                                       material,
                                                                       resolved_vil,
                                                                       primitive->GetRuntimeTextureBinding()),
                                             primitive->GetPrimitive());
            mi_binding = graph::MaterialBindingInstanceInternalAccess::GetDomainBinding(mi);
            LogInfo("[TBV][TextureMaterialBindingSystem] Final resolved state updated (non-domain): primitive=%p primitive.prim=%p mi=%p material=%p(%s) mi.binding=%p resolved_domain=%p resolved_mi_id=%d preset=%u",
                    static_cast<void *>(primitive),
                    static_cast<void *>(primitive->GetPrimitive()),
                    static_cast<void *>(mi),
                    static_cast<void *>(material),
                    material->GetName().c_str(),
                    static_cast<void *>(mi_binding),
                    static_cast<void *>(graph::MaterialBindingInstanceInternalAccess::GetDomain(mi)),
                    mi->GetMIID(),
                    static_cast<unsigned>(mi->GetRenderPreset()));
            return true;
        }

        graph::mtl::MaterialRecipe recipe;
        if (!BuildBoundRecipe(world, primitive, binding, recipe))
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] BuildBoundRecipe failed: primitive=%p texture='%s' domain_tag='%s'",
                    static_cast<void *>(primitive),
                    texture_path.c_str(),
                    binding.HasDomainTag() ? binding.domain_tag.c_str() : "");
            return false;
        }

        auto *previous_material = primitive->GetShaderMaterialProgram();
        graph::MaterialDomainHandle handle;
        const graph::VIL *resolved_vil = nullptr;
        auto &resolve_request = primitive->GetMaterialResolveRequest();
        auto *mi = recipe_registry->ResolveOrCreateBindingInstance(recipe,
                                                                   geometry->GetGeometryVertexFormat(),
                                                                   resolve_request.GetInstanceDataPtr(),
                                                                   resolve_request.GetInstanceDataSize(),
                                                                   &handle,
                                                                   &resolved_vil);
        if (!mi)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] ResolveOrCreateBindingInstance returned null: primitive=%p material_recipe_domain='%s' texture='%s'",
                    static_cast<void *>(primitive),
                    recipe.domain_id.c_str(),
                    texture_path.c_str());
            return false;
        }

        auto *material = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
        if (!material)
        {
            LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] mi has null material: mi=%p", static_cast<void *>(mi));
            return false;
        }

        auto *mi_binding = graph::MaterialBindingInstanceInternalAccess::GetDomainBinding(mi);
        LogInfo("[TBV][TextureMaterialBindingSystem] Resolved binding instance: primitive=%p geometry=%p mi=%p material=%p(%s) handle.domain=%p handle.binding=%p mi.binding=%p vil=%p domain_tag='%s' texture='%s'",
                static_cast<void *>(primitive),
                static_cast<void *>(geometry),
                static_cast<void *>(mi),
                static_cast<void *>(material),
                material ? material->GetName().c_str() : "<null>",
                static_cast<void *>(handle.domain),
                static_cast<void *>(handle.binding),
                static_cast<void *>(mi_binding),
                static_cast<const void *>(resolved_vil),
                binding.HasDomainTag() ? binding.domain_tag.c_str() : "",
                texture_path.c_str());

        auto *descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>().get();

        RuntimeTextureBinding produced_runtime_binding{};

        if (!binding.HasDomainTag())
        {
            RuntimeTextureBinding runtime_binding{};
            runtime_binding.kind = RuntimeTextureBinding::Kind::SingleTexture;
            runtime_binding.status = RuntimeTextureBinding::Status::Pending;

            LogInfo("[TBV][TextureMaterialBindingSystem] Enter non-domain branch: primitive=%p mi=%p material=%p handle.binding=%p mi.binding=%p slot=%u",
                    static_cast<void *>(primitive),
                    static_cast<void *>(mi),
                    static_cast<void *>(material),
                    static_cast<void *>(handle.binding),
                    static_cast<void *>(mi_binding),
                    static_cast<unsigned>(binding.slot));

            auto *texture = texture_manager->LoadTexture2D(texture_path, true);
            if (!texture)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] LoadTexture2D failed: texture='%s'", texture_path.c_str());
                return false;
            }

            auto *sampler = QuadResourcePrepareSystem::GetSharedSampler(); // TODO(Phase 7): replace with generic sampler manager
            if (!sampler)
                sampler = sampler_manager->CreateSampler(texture);
            if (!sampler)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] sampler is null: texture='%s'",
                        texture_path.c_str());
                return false;
            }

            if (handle.binding)
            {
                handle.binding->BindResourceSampler(binding.slot, texture, sampler);
                handle.binding->Update();

                auto *handle_pm = handle.binding->GetPerMaterialMP();
                LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain handle.binding updated: binding=%p pm=%p ds=%p tex=%p sampler=%p",
                        static_cast<void *>(handle.binding),
                        static_cast<void *>(handle_pm),
                        handle_pm ? (void *)handle_pm->GetVkDescriptorSet() : nullptr,
                        static_cast<void *>(texture),
                        static_cast<void *>(sampler));
            }
            else
            {
                LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain handle.binding is null: primitive=%p mi=%p material=%p",
                        static_cast<void *>(primitive),
                        static_cast<void *>(mi),
                        static_cast<void *>(material));
            }

            if (!material->BindResourceSampler(binding.slot, texture, sampler))
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] material->BindResourceSampler failed (non-domain): material=%p(%s) slot=%u tex=%p sampler=%p",
                        static_cast<void *>(material),
                        material ? material->GetName().c_str() : "<null>",
                        static_cast<unsigned>(binding.slot),
                        static_cast<void *>(texture),
                        static_cast<void *>(sampler));
                return false;
            }
            material->Update();

            auto *material_pm = material->GetMP(hgl::graph::DescriptorSetType::PerMaterial);
            LogInfo("[TBV][TextureMaterialBindingSystem] Non-domain material updated: material=%p(%s) pm=%p ds=%p tex=%p sampler=%p",
                    static_cast<void *>(material),
                    material ? material->GetName().c_str() : "<null>",
                    static_cast<void *>(material_pm),
                    material_pm ? (void *)material_pm->GetVkDescriptorSet() : nullptr,
                    static_cast<void *>(texture),
                    static_cast<void *>(sampler));

            if (descriptor_binding_system)
            {
                if (previous_material && previous_material != material)
                    descriptor_binding_system->ClearMaterialBindings(previous_material);

                descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                          binding.slot,
                                                                          texture,
                                                                          sampler);
            }

            math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
            mi->WriteMIData(texture_size);

            runtime_binding.texture = texture;
            runtime_binding.sampler = sampler;
            runtime_binding.domain_binding = handle.binding;
            runtime_binding.domain = handle.domain;
            runtime_binding.binding_id = g_runtime_texture_binding_id++;
            runtime_binding.status = RuntimeTextureBinding::Status::Ready;
            runtime_binding.ready = true;
            primitive->SetRuntimeTextureBinding(runtime_binding);
            produced_runtime_binding = primitive->GetRuntimeTextureBinding();
        }
        else
        {
            RuntimeTextureBinding runtime_binding{};
            runtime_binding.kind = RuntimeTextureBinding::Kind::TextureArray;
            runtime_binding.status = RuntimeTextureBinding::Status::Pending;

            LogInfo("[TBV][TextureMaterialBindingSystem] Enter domain branch: primitive=%p mi=%p material=%p handle.binding=%p mi.binding=%p slot=%u domain_tag='%s'",
                    static_cast<void *>(primitive),
                    static_cast<void *>(mi),
                    static_cast<void *>(material),
                    static_cast<void *>(handle.binding),
                    static_cast<void *>(mi_binding),
                    static_cast<unsigned>(binding.slot),
                    binding.domain_tag.c_str());

            const int layer = graph::TextureDomainRegistry::RegisterTexture(binding.domain_tag, texture_path);
            if (layer < 0)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] RegisterTexture failed: domain_tag='%s' texture='%s'",
                        binding.domain_tag.c_str(),
                        texture_path.c_str());
                return false;
            }

            auto *domain_resources = graph::TextureDomainRegistry::GetEntry(binding.domain_tag);
            // Domain resources are built by QuadResourcePrepareSystem::Update() which runs first
            // due to AddDependency<QuadResourcePrepareSystem>() in the constructor.
            // TODO(Phase 5): replace execution-order dependency with a generic DomainResourcesReadySystem.
            if (!domain_resources || domain_resources->dirty || !domain_resources->texture_array || !domain_resources->sampler)
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] domain resources not ready: domain_tag='%s' resources=%p dirty=%d texture_array=%p sampler=%p",
                        binding.domain_tag.c_str(),
                        static_cast<void *>(domain_resources),
                        domain_resources ? int(domain_resources->dirty) : -1,
                        domain_resources ? static_cast<void *>(domain_resources->texture_array) : nullptr,
                        domain_resources ? static_cast<void *>(domain_resources->sampler) : nullptr);
                return false; // not ready yet; will retry next frame
            }

            if (!EnsurePrimitiveBindingInstance(primitive_manager, primitive, mi, resolved_vil))
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] EnsurePrimitiveBindingInstance failed: primitive=%p mi=%p material=%p(%s) vil=%p",
                        static_cast<void *>(primitive),
                        static_cast<void *>(mi),
                        static_cast<void *>(material),
                        material ? material->GetName().c_str() : "<null>",
                        static_cast<const void *>(resolved_vil));
                return false;
            }

            if (handle.binding)
            {
                handle.binding->BindResourceSampler(binding.slot,
                                                    domain_resources->texture_array,
                                                    domain_resources->sampler);
                handle.binding->Update();

                auto *handle_pm = handle.binding->GetPerMaterialMP();
                LogInfo("[TBV][TextureMaterialBindingSystem] Domain handle.binding updated: binding=%p pm=%p ds=%p tex_array=%p sampler=%p layer=%d",
                        static_cast<void *>(handle.binding),
                        static_cast<void *>(handle_pm),
                        handle_pm ? (void *)handle_pm->GetVkDescriptorSet() : nullptr,
                        static_cast<void *>(domain_resources->texture_array),
                        static_cast<void *>(domain_resources->sampler),
                        layer);
            }
            else
            {
                LogInfo("[TBV][TextureMaterialBindingSystem] Domain handle.binding is null: primitive=%p mi=%p material=%p domain_tag='%s'",
                        static_cast<void *>(primitive),
                        static_cast<void *>(mi),
                        static_cast<void *>(material),
                        binding.domain_tag.c_str());
            }

            if (!material->BindResourceSampler(binding.slot,
                                               domain_resources->texture_array,
                                               domain_resources->sampler))
            {
                LogInfo("[TBV][TextureMaterialBindingSystem][FAIL] material->BindResourceSampler failed (domain): material=%p(%s) slot=%u tex_array=%p sampler=%p layer=%d domain_tag='%s'",
                        static_cast<void *>(material),
                        material ? material->GetName().c_str() : "<null>",
                        static_cast<unsigned>(binding.slot),
                        static_cast<void *>(domain_resources->texture_array),
                        static_cast<void *>(domain_resources->sampler),
                        layer,
                        binding.domain_tag.c_str());
                return false;
            }
            material->Update();

            auto *material_pm = material->GetMP(hgl::graph::DescriptorSetType::PerMaterial);
            LogInfo("[TBV][TextureMaterialBindingSystem] Domain material updated: material=%p(%s) pm=%p ds=%p tex_array=%p sampler=%p layer=%d",
                    static_cast<void *>(material),
                    material ? material->GetName().c_str() : "<null>",
                    static_cast<void *>(material_pm),
                    material_pm ? (void *)material_pm->GetVkDescriptorSet() : nullptr,
                    static_cast<void *>(domain_resources->texture_array),
                    static_cast<void *>(domain_resources->sampler),
                    layer);

            if (descriptor_binding_system)
            {
                if (previous_material && previous_material != material)
                    descriptor_binding_system->ClearMaterialBindings(previous_material);

                if (handle.binding)
                {
                    descriptor_binding_system->RegisterDomainBinding(handle.binding);
                    descriptor_binding_system->RegisterDomainTextureSampler(handle.binding,
                                                                           binding.slot,
                                                                           domain_resources->texture_array,
                                                                           domain_resources->sampler);
                }

                descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                          binding.slot,
                                                                          domain_resources->texture_array,
                                                                          domain_resources->sampler);
            }

            mi->SetTextureArrayLayer(binding.slot, static_cast<uint32_t>(layer));
            math::Vector2u texture_size(domain_resources->texture_array->GetWidth(),
                                        domain_resources->texture_array->GetHeight());
            mi->WriteMIData(texture_size);

            runtime_binding.texture = domain_resources->texture_array;
            runtime_binding.sampler = domain_resources->sampler;
            runtime_binding.domain_binding = handle.binding;
            runtime_binding.domain = handle.domain;
            runtime_binding.layer = static_cast<uint32_t>(layer);
            runtime_binding.binding_id = g_runtime_texture_binding_id++;
            runtime_binding.status = RuntimeTextureBinding::Status::Ready;
            runtime_binding.ready = true;
            primitive->SetRuntimeTextureBinding(runtime_binding);
            produced_runtime_binding = primitive->GetRuntimeTextureBinding();
        }

        UpdateResolvedMaterialState(primitive, mi, material, resolved_vil);
        primitive->SetStagingRenderState(BuildStagingMaterialState(primitive,
                                                                   mi,
                                                                   material,
                                                                   resolved_vil,
                                                                   produced_runtime_binding),
                                         primitive->GetPrimitive());
        mi_binding = graph::MaterialBindingInstanceInternalAccess::GetDomainBinding(mi);
        LogInfo("[TBV][TextureMaterialBindingSystem] Final resolved state updated: primitive=%p primitive.prim=%p mi=%p material=%p(%s) mi.binding=%p resolved_domain=%p resolved_mi_id=%d preset=%u",
                static_cast<void *>(primitive),
                static_cast<void *>(primitive->GetPrimitive()),
                static_cast<void *>(mi),
                static_cast<void *>(material),
                material ? material->GetName().c_str() : "<null>",
                static_cast<void *>(mi_binding),
                static_cast<void *>(graph::MaterialBindingInstanceInternalAccess::GetDomain(mi)),
                mi->GetMIID(),
                static_cast<unsigned>(mi->GetRenderPreset()));
        return true;
    }
}//namespace hgl::ecs
