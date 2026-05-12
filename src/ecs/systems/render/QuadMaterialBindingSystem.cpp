#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/type/StdString.h>
#include<iostream>

namespace hgl::ecs
{
    struct QuadResolvedMaterialState
    {
        graph::MaterialBindingInstance *binding_instance = nullptr;
        graph::ShaderMaterialProgram *material = nullptr;
    };

    static QuadResolvedMaterialState ResolveMaterialInstanceState(graph::MaterialBindingInstance *mi,
                                                                  graph::ShaderMaterialProgram *expected_material = nullptr)
    {
        QuadResolvedMaterialState state{};
        state.binding_instance = mi;
        state.material = expected_material;

        if (!mi)
            return state;

        return state;
    }

    static graph::ShaderMaterialProgram *ResolvePrimitiveMaterialStateFirst(graph::Primitive *prim)
    {
        if (!prim)
            return nullptr;

        auto *resolved_mi = prim->GetResolvedBindingInstance();
        const auto state = ResolveMaterialInstanceState(resolved_mi);

#ifdef _DEBUG
        if (!resolved_mi)
        {
            std::cout << "[QuadMaterialBindingSystem] DEBUG: primitive resolved binding instance is null" << std::endl;
        }
#endif

        return state.material;
    }

    static graph::mtl::MaterialRecipe BuildLegacyQuadRecipe(const QuadComponent *quad,
                                                            const graph::GraphicsPipelinePreset pipeline,
                                                            const graph::RenderAlphaMode blend_mode,
                                                            const graph::TextureChannelHint channel_hint,
                                                            const hgl::OSString &texture_path)
    {
        graph::mtl::MaterialRecipe recipe;
        recipe.id = "quad_legacy_single_texture";
        recipe.preset = quad && quad->IsFixedPixelSize()
            ? graph::mtl::MaterialPreset::Billboard2DFixed
            : graph::mtl::MaterialPreset::Billboard2DDynamic;
        recipe.dim = graph::mtl::MaterialRecipe::Dim::D3;
        recipe.prim = graph::PrimitiveType::Billboard;
        recipe.pipeline = pipeline;
        recipe.billboard.fixed_size = quad ? quad->IsFixedPixelSize() : false;
        recipe.billboard.pixel_w = quad ? quad->GetPixelSize().x : 64u;
        recipe.billboard.pixel_h = quad ? quad->GetPixelSize().y : 64u;
        recipe.billboard.blend_mode = blend_mode;
        recipe.billboard.base_color_channel = channel_hint;
        recipe.billboard.front_face_ccw = quad && quad->GetFrontFace() == VK_FRONT_FACE_COUNTER_CLOCKWISE;
        recipe.billboard.texture_id = ToStdString(texture_path);
        recipe.textures = {
            { graph::mtl::SamplerSlot::BaseColor, graph::mtl::TextureSourceMode::Simple, "" },
        };
        return recipe;
    }

    static graph::ResourceDomain *ResolveDomainForMaterial(graph::GraphicsContext *gc,
                                                           graph::ShaderMaterialProgram *material,
                                                           uint32_t domain_id)
    {
        if (!material)
            return nullptr;

        auto *rdm = gc ? gc->GetResourceDomainManager() : nullptr;
        if (!rdm)
            return nullptr;

        const auto schema = material->GetShaderDataSchema();

        if (auto *domain = rdm->Get(schema, domain_id))
            return domain;

        graph::ResourceDomainCreateInfo ci;
        ci.schema = schema;
        ci.domain_id = domain_id;
        ci.initial_capacity = 256;
        return rdm->Create(ci);
    }

    QuadMaterialBindingSystem::QuadMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Billboard");
        AddDependency<QuadResourcePrepareSystem>();
    }

    void QuadMaterialBindingSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        // Get shared primitive from QuadResourcePrepareSystem
        auto* shared_primitive = QuadResourcePrepareSystem::GetSharedPrimitive();
        if (!shared_primitive)
            return;

        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        // Pre-registration pass: register all domain textures before binding.
        // QuadResourcePrepareSystem::Update() runs earlier (RenderResourceSetup phase)
        // when s_domain_resources is still empty on the first frame, so EnsureDomainResources()
        // produces no domain materials.  By registering every domain-tagged quad's texture
        // here first and then calling EnsureDomainResources(), we guarantee that the
        // Texture2DArray and sampler2DArray ShaderMaterialProgram are ready within the
        // same frame — avoiding a one-frame fallback to the wrong sampler2D shared material.
        bool needs_domain_rebuild = false;
        for (Entity* entity : entities)
        {
            if (!entity) continue;
            auto quad = entity->GetComponent<QuadComponent>();
            if (!quad || !quad->IsVisible() || !quad->HasDomainTag()) continue;

            const auto& texture_path = quad->GetTexturePath();
            if (texture_path.IsEmpty()) continue;

            const int layer = QuadResourcePrepareSystem::RegisterDomainTexture(
                quad->GetDomainTag(), texture_path);
            if (layer >= 0)
            {
                auto* dr = QuadResourcePrepareSystem::GetDomainResources(quad->GetDomainTag());
                if (!dr || !dr->material)
                    needs_domain_rebuild = true;
            }
        }

        if (needs_domain_rebuild)
        {
            if (auto prep_sys = world->GetSystem<QuadResourcePrepareSystem>())
                prep_sys->EnsureDomainResources();
        }

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto quad = entity->GetComponent<QuadComponent>();
            if (!quad)
                continue;

            if (!quad->IsVisible())
                continue;

            // Ensure quad has the shared primitive
            auto prim = quad->GetPrimitive();
            if (!prim)
            {
                quad->SetPrimitive(shared_primitive);
                prim = shared_primitive;
            }
            if (!prim)
                continue;

            // Ensure quad has its material with texture binding
            if (!EnsureQuadMaterial(quad.get()))
                continue;
        }
    }

    bool QuadMaterialBindingSystem::EnsureQuadMaterial(QuadComponent* quad)
    {
        if (!quad || !world)
            return false;

        const auto& texture_path = quad->GetTexturePath();
        if (texture_path.IsEmpty())
            return true;

        // ── Domain texture-array path ─────────────────────────────
        if (quad->HasDomainTag())
            return EnsureQuadMaterialDomain(quad);

        // ── Legacy single-texture path ────────────────────────────

        // If texture hasn't changed and a quad-specific primitive is already bound, skip
        if (!quad->IsTextureDirty() && quad->GetPrimitive() != QuadResourcePrepareSystem::GetSharedPrimitive())
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* texture_manager = graphics_context->GetTextureManager();
        auto* recipe_registry = graphics_context->GetMaterialAssetRegistry();
        if (!material_manager || !primitive_manager || !texture_manager || !recipe_registry)
            return false;

        // Load texture
        auto* texture = texture_manager->LoadTexture2D(texture_path, true);
        if (!texture)
            return false;

        // Get shared sampler from QuadResourcePrepareSystem
        auto* shared_sampler = QuadResourcePrepareSystem::GetSharedSampler();
        if (!shared_sampler)
            return false;

        const auto pipeline = QuadResourcePrepareSystem::GetPresetForWorld(world);
        const auto blend_mode = QuadResourcePrepareSystem::GetBlendModeForWorld(world);
        const auto channel_hint = QuadResourcePrepareSystem::GetChannelHintForWorld(world);

        graph::MaterialDomainHandle handle;
        auto recipe = BuildLegacyQuadRecipe(quad, pipeline, blend_mode, channel_hint, texture_path);
        auto* mi = recipe_registry->ResolveOrCreateBindingInstance(recipe, nullptr, 0, &handle);
        if (!mi || !handle.material)
            return false;

        auto* quad_material = handle.material;

        const auto mi_state = ResolveMaterialInstanceState(mi, quad_material);
        if (!mi_state.material)
            return false;

        graph::ShaderMaterialProgram *previous_material = nullptr;
        {
            auto *current_prim = quad->GetPrimitive();
            if (current_prim && current_prim != QuadResourcePrepareSystem::GetSharedPrimitive())
                previous_material = ResolvePrimitiveMaterialStateFirst(current_prim);
        }

        mi->SetRenderPreset(QuadResourcePrepareSystem::GetPresetForWorld(world));

        graph::Primitive *current_primitive = quad->GetPrimitive();
        graph::Geometry *geometry = current_primitive ? current_primitive->GetGeometry() : nullptr;
        if (!geometry)
        {
            auto *shared_primitive = QuadResourcePrepareSystem::GetSharedPrimitive();
            geometry = shared_primitive ? shared_primitive->GetGeometry() : nullptr;
        }

        if (!geometry)
            return false;

        graph::Primitive *shared_primitive = QuadResourcePrepareSystem::GetSharedPrimitive();
        graph::Primitive *quad_primitive = nullptr;

        if (current_primitive
         && current_primitive != shared_primitive
         && ResolvePrimitiveMaterialStateFirst(current_primitive) == mi_state.material)
        {
            if (!current_primitive->ChangeMaterialInstance(mi))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            quad_primitive = primitive_manager->CreatePrimitive(geometry, mi);
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != shared_primitive)
                primitive_manager->Release(current_primitive);
        }

        graph::ShaderMaterialProgram *material = mi_state.material;
        if (!material)
        {
            return false;
        }

        if (!material->BindResourceSampler(graph::mtl::SamplerSlot::BaseColor,
                                          texture,
                                          shared_sampler))
        {
            return false;
        }

        if (auto descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            if (previous_material && previous_material != material)
                descriptor_binding_system->ClearMaterialBindings(previous_material);

            descriptor_binding_system->RegisterMaterialTextureSampler(material,
                                                                      graph::mtl::SamplerSlot::BaseColor,
                                                                      texture,
                                                                      shared_sampler);
        }

        // Write texture size to material instance data
        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi->WriteMIData(texture_size);

        // Update quad component
        quad->SetPrimitive(quad_primitive);
        quad->SetTextureObjects(texture, shared_sampler);
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }

    bool QuadMaterialBindingSystem::EnsureQuadMaterialDomain(QuadComponent* quad)
    {
        const auto& texture_path = quad->GetTexturePath();
        const auto& domain_tag   = quad->GetDomainTag();

        // Register this texture into the domain (returns layer index or -1)
        const int layer = QuadResourcePrepareSystem::RegisterDomainTexture(domain_tag, texture_path);
        if (layer < 0)
            return false;

        // Get domain resources (may not be fully built yet if dirty)
        auto* dr = QuadResourcePrepareSystem::GetDomainResources(domain_tag);
        if (!dr || !dr->material || !dr->primitive)
            return false; // will be ready next frame after EnsureDomainResources()

        // If texture hasn't changed and this quad is already using the domain primitive, skip
        if (!quad->IsTextureDirty() && quad->GetPrimitive() == dr->primitive)
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!material_manager || !primitive_manager)
            return false;

        // Create a MaterialBindingInstance from the domain's shared ShaderMaterialProgram
        graph::MaterialInstanceSpec spec;
        spec.material = dr->material;
        spec.domain   = dr->dmb ? dr->dmb->GetDomain() : nullptr;
        spec.preset   = QuadResourcePrepareSystem::GetPresetForWorld(world);
        auto* mi = material_manager->AcquireMaterialInstance(spec);
        if (!mi)
            return false;

        const auto mi_state = ResolveMaterialInstanceState(mi, dr->material);
        if (!mi_state.material)
            return false;

        mi->SetRenderPreset(QuadResourcePrepareSystem::GetPresetForWorld(world));

        // Set the texture array layer for this quad's texture
        mi->SetTextureArrayLayer(graph::mtl::SamplerSlot::BaseColor, static_cast<uint32_t>(layer));

        // Write texture size to MI data (use texture array dimensions)
        if (dr->texture_array)
        {
            math::Vector2u texture_size(dr->texture_array->GetWidth(), dr->texture_array->GetHeight());
            mi->WriteMIData(texture_size);
        }

        // Assign domain primitive or reuse current
        graph::Primitive *current_primitive = quad->GetPrimitive();
        graph::Primitive *quad_primitive = nullptr;

        if (current_primitive
         && current_primitive != dr->primitive
         && ResolvePrimitiveMaterialStateFirst(current_primitive) == mi_state.material)
        {
            if (!current_primitive->ChangeMaterialInstance(mi))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            graph::Geometry *geometry = dr->primitive->GetGeometry();
            if (!geometry)
                return false;

            quad_primitive = primitive_manager->CreatePrimitive(geometry, mi);
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != dr->primitive)
                primitive_manager->Release(current_primitive);
        }

        // Update quad component
        quad->SetPrimitive(quad_primitive);
        quad->SetTextureObjects(nullptr, dr->sampler); // texture lives in the domain array
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }
}//namespace hgl::ecs
