#include<hgl/ecs/systems/render/TextureMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>  // execution-order dependency only; no Quad API called
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/TextureBindingComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/graph/module/TextureDomainRegistry.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/mtl/MaterialRecipeStore.h>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKDomainResourceBinding.h>

namespace hgl::ecs
{
    namespace
    {
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
                                     TextureBindingComponent *binding,
                                     graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!world || !primitive || !binding)
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
            out_recipe.domain_id = binding->HasDomainTag() ? binding->GetDomainTag() : std::string();

            return PatchTextureSlotRecipe(out_recipe,
                                          binding->GetSamplerSlot(),
                                          binding->HasDomainTag() ? graph::mtl::TextureSourceMode::Array
                                                                  : binding->GetTextureSourceMode(),
                                          binding->HasDomainTag() ? std::string()
                                                                  : ToStdString(binding->GetTexturePath()));
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

    void TextureMaterialBindingSystem::Update(float)
    {
        if (!world)
            return;

        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto primitive = entity->GetComponent<PrimitiveComponent>();
            auto binding = entity->GetComponent<TextureBindingComponent>();
            if (!primitive || !binding)
                continue;

            if (!primitive->IsVisible() || !binding->IsEnabled())
                continue;

            EnsurePrimitiveTextureBinding(primitive.get(), binding.get());
        }
    }

    bool TextureMaterialBindingSystem::EnsurePrimitiveTextureBinding(PrimitiveComponent *primitive,
                                                                     TextureBindingComponent *binding)
    {
        if (!world || !primitive || !binding)
            return false;

        const auto &texture_path = binding->GetTexturePath();
        if (texture_path.IsEmpty())
            return true;

        if (!binding->IsTextureDirty() && binding->GetAppliedTexturePath() == texture_path)
            return true;

        auto *graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto *texture_manager = graphics_context->GetTextureManager();
        auto *primitive_manager = graphics_context->GetPrimitiveManager();
        auto *recipe_registry = graphics_context->GetMaterialAssetRegistry();
        if (!texture_manager || !primitive_manager || !recipe_registry)
            return false;

        graph::Geometry *geometry = primitive->GetPrimitive() ? primitive->GetPrimitive()->GetGeometry()
                                                              : primitive->GetUnresolvedGeometry();
        if (!geometry)
            return false;

        graph::mtl::MaterialRecipe recipe;
        if (!BuildBoundRecipe(world, primitive, binding, recipe))
            return false;

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
            return false;

        auto *material = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
        if (!material)
            return false;

        if (!EnsurePrimitiveBindingInstance(primitive_manager, primitive, mi, resolved_vil))
            return false;

        auto *descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>().get();

        if (!binding->HasDomainTag())
        {
            auto *texture = texture_manager->LoadTexture2D(texture_path, true);
            if (!texture)
                return false;

            auto *sampler = QuadResourcePrepareSystem::GetSharedSampler(); // TODO(Phase 7): replace with generic sampler manager
            if (!sampler)
                sampler = binding->GetSampler();
            if (!sampler)
                return false;

            if (handle.binding)
            {
                handle.binding->BindResourceSampler(binding->GetSamplerSlot(), texture, sampler);
                handle.binding->Update();
            }

            if (!material->BindResourceSampler(binding->GetSamplerSlot(), texture, sampler))
                return false;
            material->Update();

            if (descriptor_binding_system)
            {
                if (previous_material && previous_material != material)
                    descriptor_binding_system->ClearMaterialBindings(previous_material);

                descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                          binding->GetSamplerSlot(),
                                                                          texture,
                                                                          sampler);
            }

            math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
            mi->WriteMIData(texture_size);
            binding->SetTextureObjects(texture, sampler);
        }
        else
        {
            const int layer = graph::TextureDomainRegistry::RegisterTexture(binding->GetDomainTag(), texture_path);
            if (layer < 0)
                return false;

            auto *domain_resources = graph::TextureDomainRegistry::GetEntry(binding->GetDomainTag());
            // Domain resources are built by QuadResourcePrepareSystem::Update() which runs first
            // due to AddDependency<QuadResourcePrepareSystem>() in the constructor.
            // TODO(Phase 5): replace execution-order dependency with a generic DomainResourcesReadySystem.
            if (!domain_resources || domain_resources->dirty || !domain_resources->texture_array || !domain_resources->sampler)
                return false; // not ready yet; will retry next frame

            if (handle.binding)
            {
                handle.binding->BindResourceSampler(binding->GetSamplerSlot(),
                                                    domain_resources->texture_array,
                                                    domain_resources->sampler);
                handle.binding->Update();
            }

            if (!material->BindResourceSampler(binding->GetSamplerSlot(),
                                               domain_resources->texture_array,
                                               domain_resources->sampler))
                return false;
            material->Update();

            if (descriptor_binding_system)
            {
                if (previous_material && previous_material != material)
                    descriptor_binding_system->ClearMaterialBindings(previous_material);

                if (handle.binding)
                {
                    descriptor_binding_system->RegisterDomainBinding(handle.binding);
                    descriptor_binding_system->RegisterDomainTextureSampler(handle.binding,
                                                                           binding->GetSamplerSlot(),
                                                                           domain_resources->texture_array,
                                                                           domain_resources->sampler);
                }

                descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                          binding->GetSamplerSlot(),
                                                                          domain_resources->texture_array,
                                                                          domain_resources->sampler);
            }

            mi->SetTextureArrayLayer(binding->GetSamplerSlot(), static_cast<uint32_t>(layer));
            math::Vector2u texture_size(domain_resources->texture_array->GetWidth(),
                                        domain_resources->texture_array->GetHeight());
            mi->WriteMIData(texture_size);
            binding->SetTextureObjects(nullptr, domain_resources->sampler);
        }

        UpdateResolvedMaterialState(primitive, mi, material, resolved_vil);
        binding->SetAppliedTexturePath(texture_path);
        binding->ClearTextureDirty();
        return true;
    }
}//namespace hgl::ecs
