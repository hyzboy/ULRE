#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>
#include<cstdint>
#include<unordered_map>

namespace hgl::ecs
{
    // Static member initialization
    graph::Primitive* QuadResourcePrepareSystem::shared_primitive = nullptr;
    graph::MaterialInstance* QuadResourcePrepareSystem::shared_material_instance = nullptr;
    graph::Pipeline* QuadResourcePrepareSystem::shared_pipeline = nullptr;
    graph::RenderPass* QuadResourcePrepareSystem::shared_render_pass = nullptr;
    graph::Sampler* QuadResourcePrepareSystem::shared_sampler = nullptr;

    static graph::InlinePipeline g_default_quad_inline_pipeline = graph::InlinePipeline::Solid3D;
    static std::unordered_map<const ECSContext*, graph::InlinePipeline> g_world_quad_inline_pipeline;
    static std::unordered_map<const ECSContext*, graph::TextureChannelHint> g_world_quad_channel_hint;

    static graph::BlendMode InlinePipelineToBlendMode(graph::InlinePipeline pipeline)
    {
        switch (pipeline)
        {
        case graph::InlinePipeline::Masked3D:          return graph::BlendMode::Masked;
        case graph::InlinePipeline::Dither3D:          return graph::BlendMode::Dither;
        default:                                        return graph::BlendMode::Transparent;
        }
    }

    void QuadResourcePrepareSystem::SetPipelineForWorld(const ECSContext* world,
                                                        graph::InlinePipeline pipeline)
    {
        if (!world)
            return;

        g_world_quad_inline_pipeline[world] = pipeline;
    }

    graph::InlinePipeline QuadResourcePrepareSystem::GetPipelineForWorld(const ECSContext* world)
    {
        if (world)
        {
            auto it = g_world_quad_inline_pipeline.find(world);
            if (it != g_world_quad_inline_pipeline.end())
                return it->second;
        }

        return g_default_quad_inline_pipeline;
    }

    graph::BlendMode QuadResourcePrepareSystem::GetBlendModeForWorld(const ECSContext* world)
    {
        return InlinePipelineToBlendMode(GetPipelineForWorld(world));
    }

    void QuadResourcePrepareSystem::SetChannelHintForWorld(const ECSContext* world,
                                                           graph::TextureChannelHint hint)
    {
        if (!world)
            return;

        g_world_quad_channel_hint[world] = hint;
    }

    graph::TextureChannelHint QuadResourcePrepareSystem::GetChannelHintForWorld(const ECSContext* world)
    {
        if (world)
        {
            auto it = g_world_quad_channel_hint.find(world);
            if (it != g_world_quad_channel_hint.end())
                return it->second;
        }

        return graph::TextureChannelHint::RGBA;
    }

    void QuadResourcePrepareSystem::SetPipeline(graph::InlinePipeline pipeline)
    {
        g_default_quad_inline_pipeline = pipeline;
    }

    graph::InlinePipeline QuadResourcePrepareSystem::GetPipeline()
    {
        return g_default_quad_inline_pipeline;
    }

    graph::BlendMode QuadResourcePrepareSystem::GetBlendMode()
    {
        return InlinePipelineToBlendMode(g_default_quad_inline_pipeline);
    }

    graph::Pipeline* QuadResourcePrepareSystem::CreateConfiguredPipeline(graph::RenderPass* render_pass,
                                                                         graph::MaterialInstance* material_instance,
                                                                         const ECSContext* world)
    {
        if (!render_pass || !material_instance)
            return nullptr;

        return render_pass->CreatePipeline(material_instance, GetPipelineForWorld(world));
    }

    QuadResourcePrepareSystem::QuadResourcePrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Material);
        SetExecutionOrder(ExecutionPhase::RenderResourceSetup);
        SetRenderElementType("Billboard");
        AddDependency<RenderTargetSystem>();
    }

    void QuadResourcePrepareSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        // Ensure shared resources are created and up-to-date
        EnsureSharedResources();
    }

    void QuadResourcePrepareSystem::Shutdown()
    {
        if (world)
        {
            g_world_quad_inline_pipeline.erase(world);
            g_world_quad_channel_hint.erase(world);
        }

        ReleaseSharedResources();
        System::Shutdown();
    }

    bool QuadResourcePrepareSystem::EnsureSharedResources()
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

        // If resources already exist and render pass hasn't changed, we're done
        if (shared_primitive && shared_render_pass == render_pass)
            return true;

        // Create shared material instance for billboard rendering
        graph::mtl::BillboardMaterialCreateConfig cfg(graph::PrimitiveType::Billboard);
        cfg.fixed_size  = true;
        cfg.blend_mode  = GetBlendModeForWorld(world);
        cfg.base_color_channel = GetChannelHintForWorld(world);

        shared_material_instance = material_manager->CreateMaterialInstance(graph::mtl::MaterialPreset::Billboard2D, &cfg);
        if (!shared_material_instance)
            return false;

        // Create pipeline according to the configured quad pipeline mode
        shared_pipeline = CreateConfiguredPipeline(render_pass, shared_material_instance, world);
        if (!shared_pipeline)
            return false;

        // Create shared quad geometry (explicit quad for VS/FS-only billboard path)
        auto pc = std::make_unique<graph::GeometryCreater>(device, shared_material_instance->GetVIL());
        pc->Init("Quad", 4, 6, graph::IndexType::U16);

        static const float position_data[12] =
        {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        static const uint16_t index_data[6] = { 0, 1, 2, 0, 2, 3 };

        if (!pc->WriteVAB(graph::VAN::Position, VF_V3F, position_data))
            return false;

        if (!pc->WriteIBO(index_data))
            return false;

        shared_primitive = primitive_manager->CreatePrimitive(pc.get(), shared_material_instance, shared_pipeline);
        if (!shared_primitive)
            return false;

        // Create shared sampler
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager)
            return false;

        if (!shared_sampler)
            shared_sampler = sampler_manager->CreateSampler();

        if (!shared_sampler)
            return false;

        shared_render_pass = render_pass;
        return true;
    }

    void QuadResourcePrepareSystem::ReleaseSharedResources()
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
