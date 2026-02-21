#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/SamplerName.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/TextureManager.h>


namespace hgl::ecs
{
    QuadMaterialBindingSystem::QuadMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Material);
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame_QuadMaterialBindingSystem);
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
        auto* texture_manager = graphics_context->GetTextureManager();
        if (!material_manager || !texture_manager)
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
        cfg.fixed_size = quad->IsFixedPixelSize();
        cfg.front_face = quad->GetFrontFace();
        cfg.pixel_size = quad->GetPixelSize();

        auto* mi = material_manager->CreateMaterialInstance(graph::mtl::inline_material::Billboard2D, &cfg);
        if (!mi)
            return false;

        // Bind texture to material
        if (!mi->GetMaterial()->BindTextureSampler(graph::DescriptorSetType::PerMaterial,
                                                   graph::mtl::SamplerName::BaseColor,
                                                   texture,
                                                   shared_sampler))
        {
            return false;
        }

        // Write texture size to material instance data
        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi->WriteMIData(texture_size);

        // Update quad component
        quad->SetOverrideMaterial(mi);
        quad->SetTextureObjects(texture, shared_sampler);
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }
}//namespace hgl::ecs
