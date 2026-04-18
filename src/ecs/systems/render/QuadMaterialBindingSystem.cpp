#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
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

    static QuadResolvedMaterialState ResolveMaterialInstanceState(graph::ShaderMaterialProgram *fallback_material,
                                                                 graph::MaterialBindingInstance *mi)
    {
        QuadResolvedMaterialState state{};
        state.binding_instance = mi;
        state.material = fallback_material;

        if (!mi)
            return state;

        if (auto *mi_material = mi->GetShaderMaterialProgram())
        {
#ifdef _DEBUG
            if (state.material && state.material != mi_material)
            {
                std::cout << "[QuadMaterialBindingSystem] DEBUG: fallback material and MI material mismatch" << std::endl;
            }
#endif
            state.material = mi_material;
        }

        return state;
    }

    static graph::ShaderMaterialProgram *ResolvePrimitiveMaterialStateFirst(graph::Primitive *prim)
    {
        if (!prim)
            return nullptr;

        auto *resolved_mi = prim->GetResolvedBindingInstance();
        auto *state_material = resolved_mi ? resolved_mi->GetShaderMaterialProgram() : nullptr;

#ifdef _DEBUG
        auto *legacy_material = prim->GetShaderMaterialProgram();
        if (state_material && legacy_material && state_material != legacy_material)
        {
            std::cout << "[QuadMaterialBindingSystem] DEBUG: primitive material mismatch between resolved MI and legacy accessor" << std::endl;
        }
#endif

        return state_material ? state_material : prim->GetShaderMaterialProgram();
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
        if (!material_manager || !primitive_manager || !texture_manager)
            return false;

        // Load texture
        auto* texture = texture_manager->LoadTexture2D(texture_path, true);
        if (!texture)
            return false;

        // Get shared sampler from QuadResourcePrepareSystem
        auto* shared_sampler = QuadResourcePrepareSystem::GetSharedSampler();
        if (!shared_sampler)
            return false;

        // Create material instance with quad-specific config
        graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
        cfg.fixed_size  = quad->IsFixedPixelSize();
        cfg.front_face  = quad->GetFrontFace();
        cfg.pixel_size  = quad->GetPixelSize();
        cfg.texture_id  = ToStdString(texture_path);
        cfg.blend_mode  = QuadResourcePrepareSystem::GetBlendModeForWorld(world);
        cfg.base_color_channel = QuadResourcePrepareSystem::GetChannelHintForWorld(world);

        const auto preset = cfg.fixed_size
            ? graph::mtl::MaterialPreset::Billboard2DFixed
            : graph::mtl::MaterialPreset::Billboard2DDynamic;

        auto* quad_material = material_manager->ResolveOrCreateProgram(preset, &cfg);
        if (!quad_material)
            return false;

        graph::MaterialInstanceSpec spec;
        spec.material = quad_material;
        spec.domain   = ResolveDomainForMaterial(graphics_context, quad_material, 2u);
        spec.preset = QuadResourcePrepareSystem::GetPresetForWorld(world);
        auto* mi = material_manager->AcquireMaterialInstance(spec);
        if (!mi)
            return false;

        const auto mi_state = ResolveMaterialInstanceState(quad_material, mi);
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

        // If texture hasn't changed and a quad-specific primitive is already bound, skip
        if (!quad->IsTextureDirty() && quad->GetPrimitive() != dr->primitive)
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

        const auto mi_state = ResolveMaterialInstanceState(dr->material, mi);
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
