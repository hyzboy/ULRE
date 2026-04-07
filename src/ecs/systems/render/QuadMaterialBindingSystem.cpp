#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
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

        // If texture hasn't changed and material already exists, skip
        if (!quad->IsTextureDirty() && quad->GetOverrideMaterial())
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

        graph::MaterialInstanceSpec spec;
        spec.material = quad_material;
        graph::GraphicsPipelinePreset current_preset = QuadResourcePrepareSystem::GetPresetForWorld(world);
        spec.preset = current_preset;
        auto* mi = material_manager->AcquireMaterialInstance(spec);
        if (!mi)
            return false;

        graph::MaterialTemplate *previous_material = nullptr;
        if (auto *previous_mi = quad->GetOverrideMaterial())
            // Phase B: use direct material reference instead of MI getter
            previous_material = previous_mi->GetDomain() ? quad_material : nullptr;

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
         && current_primitive->GetMaterial() == quad_material)
        {
            // Phase B: construct slot from known values, not from MI getter
            const graph::VIL *use_vil = quad_material->GetDefaultVIL();
            hgl::graph::PrimitiveMaterialSlot slot{quad_material, mi->GetDomain(), mi->GetMIID(), use_vil, current_preset};
            if (!current_primitive->BindMaterialSlot(slot))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            quad_primitive = primitive_manager->CreatePrimitive(geometry, mi->ToSlot());
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
        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi->WriteMIData(texture_size);

        // Update quad component
        quad->SetPrimitive(quad_primitive);
        quad->SetOverrideMaterial(mi);
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
        if (!quad->IsTextureDirty() && quad->GetOverrideMaterial())
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!material_manager || !primitive_manager)
            return false;

        // Create a MaterialInstance from the domain's shared MaterialTemplate
        graph::MaterialInstanceSpec spec;
        spec.material = dr->material;
        graph::GraphicsPipelinePreset current_preset = QuadResourcePrepareSystem::GetPresetForWorld(world);
        spec.preset = current_preset;
        auto* mi = material_manager->AcquireMaterialInstance(spec);
        if (!mi)
            return false;

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
         && current_primitive->GetMaterial() == dr->material)
        {
            // Phase B: construct slot from known values
            const graph::VIL *use_vil = dr->material->GetDefaultVIL();
            hgl::graph::PrimitiveMaterialSlot slot{dr->material, mi->GetDomain(), mi->GetMIID(), use_vil, current_preset};
            if (!current_primitive->BindMaterialSlot(slot))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            graph::Geometry *geometry = dr->primitive->GetGeometry();
            if (!geometry)
                return false;

            quad_primitive = primitive_manager->CreatePrimitive(geometry, mi->ToSlot());
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != dr->primitive)
                primitive_manager->Release(current_primitive);
        }

        // Update quad component
        quad->SetPrimitive(quad_primitive);
        quad->SetOverrideMaterial(mi);
        quad->SetTextureObjects(nullptr, dr->sampler); // texture lives in the domain array
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }
}//namespace hgl::ecs
