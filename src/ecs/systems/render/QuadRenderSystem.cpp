#include<hgl/ecs/systems/render/QuadRenderSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/QuadComponent.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/SamplerName.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>

namespace hgl::ecs
{
    graph::Primitive* QuadRenderSystem::shared_primitive = nullptr;
    graph::MaterialInstance* QuadRenderSystem::shared_material_instance = nullptr;
    graph::Pipeline* QuadRenderSystem::shared_pipeline = nullptr;
    graph::RenderPass* QuadRenderSystem::shared_render_pass = nullptr;
    graph::Sampler* QuadRenderSystem::shared_sampler = nullptr;

    QuadRenderSystem::QuadRenderSystem(const std::string& name)
        : System(name)
    {
    }

    void QuadRenderSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        if (!EnsureSharedResources())
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

            auto prim = quad->GetPrimitive();
            if (!prim && shared_primitive)
            {
                quad->SetPrimitive(shared_primitive);
                prim = shared_primitive;
            }
            if (!prim)
                continue;

            if (!EnsureQuadMaterial(quad.get()))
                continue;
        }
    }

    void QuadRenderSystem::Shutdown()
    {
        ReleaseSharedResources();
        System::Shutdown();
    }

    bool QuadRenderSystem::EnsureSharedResources()
    {
        if (!world)
            return false;

        auto* render_context = world->GetRenderContext();
        auto* graphics_context = world->GetGraphicsContext();
        if (!render_context || !graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !primitive_manager || !device)
            return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if (!render_pass)
            return false;

        if (shared_primitive && shared_render_pass == render_pass)
            return true;

        graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
        cfg.fixed_size = true;

        shared_material_instance = material_manager->CreateMaterialInstance(graph::mtl::inline_material::Billboard2D, &cfg);
        if (!shared_material_instance)
            return false;

        shared_pipeline = render_pass->CreatePipeline(shared_material_instance, graph::InlinePipeline::Solid3D);
        if (!shared_pipeline)
            return false;

        auto pc = std::make_unique<graph::GeometryCreater>(device, shared_material_instance->GetVIL());
        pc->Init("Quad", 1);

        static float position_data[3] = { 0.0f, 0.0f, 0.0f };
        if (!pc->WriteVAB(graph::VertexAttribName::Position, VF_V3F, position_data))
            return false;

        shared_primitive = primitive_manager->CreatePrimitive(pc.get(), shared_material_instance, shared_pipeline);
        if (!shared_primitive)
            return false;

        shared_render_pass = render_pass;
        return true;
    }

    bool QuadRenderSystem::EnsureQuadMaterial(QuadComponent* quad)
    {
        if (!quad || !world)
            return false;

        const auto& texture_path = quad->GetTexturePath();
        if (texture_path.IsEmpty())
            return true;

        if (!quad->IsTextureDirty() && quad->GetOverrideMaterial())
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* texture_manager = graphics_context->GetTextureManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager || !texture_manager || !sampler_manager)
            return false;

        auto* texture = texture_manager->LoadTexture2D(texture_path, true);
        if (!texture)
            return false;

        if (!shared_sampler)
            shared_sampler = sampler_manager->CreateSampler();

        if (!shared_sampler)
            return false;

        graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
        cfg.fixed_size = quad->IsFixedPixelSize();
        cfg.front_face = quad->GetFrontFace();
        cfg.pixel_size = quad->GetPixelSize();

        auto* mi = material_manager->CreateMaterialInstance(graph::mtl::inline_material::Billboard2D, &cfg);
        if (!mi)
            return false;

        if (!mi->GetMaterial()->BindTextureSampler(graph::DescriptorSetType::PerMaterial,
                                                   graph::mtl::SamplerName::BaseColor,
                                                   texture,
                                                   shared_sampler))
        {
            return false;
        }

        math::Vector2u texture_size(texture->GetWidth(), texture->GetHeight());
        mi->WriteMIData(texture_size);

        quad->SetOverrideMaterial(mi);
        quad->SetTextureObjects(texture, shared_sampler);
        quad->SetAppliedTexturePath(texture_path);
        return true;
    }

    void QuadRenderSystem::ReleaseSharedResources()
    {
        if (!world)
            return;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();

        graph::Geometry* geometry = nullptr;
        if (shared_primitive)
            geometry = shared_primitive->GetGeometry();

        if (shared_primitive && primitive_manager)
            primitive_manager->Release(shared_primitive);

        if (geometry)
            delete geometry;

        if (shared_material_instance && material_manager)
            material_manager->Destroy(shared_material_instance);

        if (shared_sampler && sampler_manager)
            sampler_manager->Release(shared_sampler);

        shared_primitive = nullptr;
        shared_material_instance = nullptr;
        shared_pipeline = nullptr;
        shared_render_pass = nullptr;
        shared_sampler = nullptr;
    }
}//namespace hgl::ecs
