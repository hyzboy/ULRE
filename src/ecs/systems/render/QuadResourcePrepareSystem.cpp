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
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/ecs/support/PipelineResolveMetrics.h>
#include<cstdint>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
        PipelineResolveCounters g_quad_pipeline_resolve_counters;
    }

    // Static member initialization
    graph::Primitive* QuadResourcePrepareSystem::shared_primitive = nullptr;
    graph::MaterialInstance* QuadResourcePrepareSystem::shared_material_instance = nullptr;
    graph::GraphicsPipeline* QuadResourcePrepareSystem::shared_pipeline = nullptr;
    graph::RenderTargetFormat* QuadResourcePrepareSystem::shared_render_pass = nullptr;
    graph::Sampler* QuadResourcePrepareSystem::shared_sampler = nullptr;

    static graph::GraphicsPipelinePreset g_default_quad_inline_pipeline = graph::GraphicsPipelinePreset::Solid3D;
    static std::unordered_map<const ECSContext*, graph::GraphicsPipelinePreset> g_world_quad_inline_pipeline;
    static std::unordered_map<const ECSContext*, graph::TextureChannelHint> g_world_quad_channel_hint;

    static graph::BlendMode GraphicsPipelinePresetToBlendMode(graph::GraphicsPipelinePreset pipeline)
    {
        switch (pipeline)
        {
        case graph::GraphicsPipelinePreset::Masked3D:          return graph::BlendMode::Masked;
        case graph::GraphicsPipelinePreset::Dither3D:          return graph::BlendMode::Dither;
        default:                                        return graph::BlendMode::Transparent;
        }
    }

    void QuadResourcePrepareSystem::SetPipelineForWorld(const ECSContext* world,
                                                        graph::GraphicsPipelinePreset pipeline)
    {
        if (!world)
            return;

        g_world_quad_inline_pipeline[world] = pipeline;
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPipelineForWorld(const ECSContext* world)
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
        return GraphicsPipelinePresetToBlendMode(GetPipelineForWorld(world));
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

    void QuadResourcePrepareSystem::SetPipeline(graph::GraphicsPipelinePreset pipeline)
    {
        g_default_quad_inline_pipeline = pipeline;
    }

    graph::GraphicsPipelinePreset QuadResourcePrepareSystem::GetPipeline()
    {
        return g_default_quad_inline_pipeline;
    }

    graph::BlendMode QuadResourcePrepareSystem::GetBlendMode()
    {
        return GraphicsPipelinePresetToBlendMode(g_default_quad_inline_pipeline);
    }

    graph::GraphicsPipeline* QuadResourcePrepareSystem::CreateConfiguredPipeline(graph::RenderTargetFormat* render_pass,
                                                                         graph::MaterialInstance* material_instance,
                                                                         const ECSContext* world)
    {
        if (!render_pass || !material_instance)
            return nullptr;

        RecordPipelineResolveAttempt(g_quad_pipeline_resolve_counters);
        const uint64_t vkcreate_before = graph::RenderTargetFormat::GetVkCreateCount();

        graph::GraphicsPipeline* pipeline = render_pass->CreatePipeline(material_instance, GetPipelineForWorld(world));

        const uint64_t vkcreate_after = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t vkcreate_delta = vkcreate_after - vkcreate_before;

        if (!pipeline)
        {
            const uint64_t failures = RecordPipelineResolveFailure(g_quad_pipeline_resolve_counters);
            if (ShouldLogPow2(failures))
            {
                GLogWarning("[QuadResourcePrepareSystem] GraphicsPipeline resolve failed: total_failures=%llu",
                            static_cast<unsigned long long>(failures));
            }
            return nullptr;
        }

        RecordPipelineResolveSuccess(g_quad_pipeline_resolve_counters);
        if (ShouldLogPipelineResolveCreated(vkcreate_delta))
        {
            GLogInfo("[QuadResourcePrepareSystem] GraphicsPipeline resolve created vk pipelines=%llu (attempts=%llu successes=%llu failures=%llu)",
                     static_cast<unsigned long long>(vkcreate_delta),
                     static_cast<unsigned long long>(g_quad_pipeline_resolve_counters.attempts.load()),
                     static_cast<unsigned long long>(g_quad_pipeline_resolve_counters.successes.load()),
                     static_cast<unsigned long long>(g_quad_pipeline_resolve_counters.failures.load()));
        }

        return pipeline;
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
        auto* render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
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

        shared_material_instance = material_manager->CreateMaterialInstance(graph::mtl::MaterialPreset::Billboard2DFixed, &cfg);
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
