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
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/type/StdString.h>

namespace hgl::ecs
{
    QuadMaterialBindingSystem::QuadMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Material);
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

        auto* mi = material_manager->CreateMaterialInstance(preset, &cfg);
        if (!mi)
            return false;

        graph::Material *previous_material = nullptr;
        if (auto *previous_mi = quad->GetOverrideMaterial())
            previous_material = previous_mi->GetMaterial();

        auto *render_context = world->GetRenderContext();
        auto *render_target = render_context ? render_context->GetCurrentRenderTarget() : nullptr;
        auto *render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
        if (!render_pass)
            return false;

        auto *pipeline = QuadResourcePrepareSystem::CreateConfiguredPipeline(render_pass, mi, world);
        if (!pipeline)
            return false;

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
         && current_primitive->GetMaterial() == mi->GetMaterial())
        {
            current_primitive->UpdatePipeline(pipeline);
            if (!current_primitive->ChangeMaterialInstance(mi))
                return false;

            quad_primitive = current_primitive;
        }
        else
        {
            quad_primitive = primitive_manager->CreatePrimitive(geometry, mi, pipeline);
            if (!quad_primitive)
                return false;

            if (current_primitive && current_primitive != shared_primitive)
                primitive_manager->Release(current_primitive);
        }

        graph::Material *material = mi->GetMaterial();
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
}//namespace hgl::ecs
