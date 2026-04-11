#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/type/StdString.h>

namespace hgl::ecs
{
    namespace
    {
        bool IsDomainDirectCollectEnabled(ECSContext *world)
        {
            if (!world)
                return false;

            if (auto collect = world->GetSystem<RenderPrimitiveCollectSystem>())
                return collect->GetDomainDirectMISsboEnabled();

            return false;
        }
    }

    QuadMaterialBindingSystem::QuadMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::MaterialTemplate);
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

        // If texture hasn't changed and a dedicated primitive already exists, skip
        auto *early_shared = QuadResourcePrepareSystem::GetSharedPrimitive();
        if (!quad->IsTextureDirty() && quad->GetPrimitive() && quad->GetPrimitive() != early_shared)
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

        auto* quad_material = material_manager->AcquireMaterial(preset, &cfg);
        if (!quad_material)
            return false;

        graph::GraphicsPipelinePreset current_preset = QuadResourcePrepareSystem::GetPresetForWorld(world);
        auto *domain = material_manager->GetOrCreateDefaultDomain(quad_material);
        if (!domain)
            return false;

        const hgl::graph::PrimitiveMaterialSlot slot = material_manager->AllocMaterialInstanceSlot(
            domain,
            quad_material,
            quad_material->GetDefaultVIL(),
            current_preset,
            nullptr,
            0);
        if (!slot.IsValid())
            return false;

        graph::MaterialTemplate *previous_material = nullptr;
        {
            auto *prev_prim = quad->GetPrimitive();
            if (prev_prim && prev_prim != early_shared)
                previous_material = prev_prim->GetDomain() ? quad_material : nullptr;
        }

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
        const bool domain_direct_collect_enabled = IsDomainDirectCollectEnabled(world);

        if (current_primitive
         && current_primitive != shared_primitive
         && current_primitive->GetMaterial() == quad_material
         && !domain_direct_collect_enabled)
        {
            if (!current_primitive->BindMaterialSlot(slot,"quad"))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            quad_primitive = primitive_manager->CreatePrimitive(geometry, slot);
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != shared_primitive)
                primitive_manager->Release(current_primitive);
        }

        // Phase B: use quad_material directly
        graph::MaterialTemplate *material = quad_material;
        if (!material)
        {
            return false;
        }

        if (!material->BindTextureSampler(graph::mtl::SamplerSlot::BaseColor,
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
        {
            math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
            quad_primitive->WriteMIData(texture_size);
        }

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

        // If texture hasn't changed and material already exists, skip
        if (!quad->IsTextureDirty() && quad->GetPrimitive() && quad->GetPrimitive() != dr->primitive)
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!material_manager || !primitive_manager)
            return false;

        // Create a MaterialInstance from the domain's shared MaterialTemplate
        graph::GraphicsPipelinePreset current_preset = QuadResourcePrepareSystem::GetPresetForWorld(world);
        auto *domain = dr->dmb ? dr->dmb->GetDomain() : nullptr;
        if (!domain)
            return false;

        const hgl::graph::PrimitiveMaterialSlot slot = material_manager->AllocMaterialInstanceSlot(
            domain,
            dr->material,
            dr->material->GetDefaultVIL(),
            current_preset,
            nullptr,
            0);
        if (!slot.IsValid())
            return false;

        const uint8_t texture_array_slot_flags = dr->material->GetTextureArraySlotFlags();

        // Assign domain primitive or reuse current
        graph::Primitive *current_primitive = quad->GetPrimitive();
        graph::Primitive *quad_primitive = nullptr;
        const bool domain_direct_collect_enabled = IsDomainDirectCollectEnabled(world);

        if (current_primitive
         && current_primitive != dr->primitive
         && current_primitive->GetMaterial() == dr->material
         && !domain_direct_collect_enabled)
        {
            if (!current_primitive->BindMaterialSlot(slot,"quad"))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            graph::Geometry *geometry = dr->primitive->GetGeometry();
            if (!geometry)
                return false;

            quad_primitive = primitive_manager->CreatePrimitive(geometry, slot);
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != dr->primitive)
                primitive_manager->Release(current_primitive);
        }

        if (texture_array_slot_flags)
        {
            quad_primitive->InitMITLayout(texture_array_slot_flags);
            quad_primitive->SetTextureArrayLayer(graph::mtl::SamplerSlot::BaseColor,
                                                 static_cast<uint32_t>(layer));
        }

            // Write texture size to primitive data (use texture array dimensions)
            if (dr->texture_array)
            {
                math::Vector2u texture_size(dr->texture_array->GetWidth(), dr->texture_array->GetHeight());
                quad_primitive->WriteMIData(texture_size);
            }

        // Update quad component
        quad->SetPrimitive(quad_primitive);
        quad->SetTextureObjects(nullptr, dr->sampler); // texture lives in the domain array
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }
}//namespace hgl::ecs
