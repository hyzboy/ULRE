#include<hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/vk/VKFormat.h>
#include<cstdint>
#include<memory>
#include<cstdio>

namespace hgl::ecs
{
    namespace
    {
        static const float SPRITE2D_POS[8] =
        {
            -0.5f, -0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f,
        };

        // Vulkan UV convention: V=0 at top, V=1 at bottom
        static const float SPRITE2D_UV[8] =
        {
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
        };

        static const uint16_t SPRITE2D_IB[6] = { 0, 1, 2, 0, 2, 3 };
    }

    Sprite2DResourcePrepareSystem::Sprite2DResourcePrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderResourceSetup);
        SetRenderElementType("Sprite2D");
        AddDependency<RenderTargetSystem>();
    }

    void Sprite2DResourcePrepareSystem::Update(float deltaTime)
    {
        if (!world)
            return;

        EnsureSharedResources();
    }

    void Sprite2DResourcePrepareSystem::Shutdown()
    {
        delete shared_unit_square_geometry;
        shared_unit_square_geometry = nullptr;

        if (shared_sampler && world)
        {
            auto* gc = world->GetGraphicsContext();
            auto* sampler_manager = gc ? gc->GetSamplerManager() : nullptr;
            if (sampler_manager)
                sampler_manager->Release(shared_sampler);
        }
        shared_sampler = nullptr;

        System::Shutdown();
    }

    bool Sprite2DResourcePrepareSystem::EnsureSharedResources()
    {
        if (shared_unit_square_geometry)
        {
            // Only log once (when sampler also missing)
            if (!shared_sampler)
                std::fprintf(stderr, "[Sprite2DResPrepare] geometry OK but sampler still null\n");
            return shared_sampler != nullptr;
        }

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        graph::GeometryVertexFormat gvf;
        gvf.Set(graph::VAN::Position, VF_V2F);
        gvf.Set(graph::VAN::TexCoord, VF_V2F);

        auto pc = std::make_unique<graph::GeometryCreater>(device, gvf);
        if (!pc->Init("Sprite2DUnitSquare", 4, 6, graph::IndexType::U16))
            return false;

        if (!pc->WriteVAB(graph::VAN::Position, VF_V2F, SPRITE2D_POS))
            return false;

        if (!pc->WriteVAB(graph::VAN::TexCoord, VF_V2F, SPRITE2D_UV))
            return false;

        if (!pc->WriteIBO(SPRITE2D_IB))
            return false;

        shared_unit_square_geometry = pc->Create();
        if (!shared_unit_square_geometry)
        {
            std::fprintf(stderr, "[Sprite2DResPrepare] GeometryCreater::Create FAILED\n");
            return false;
        }
        std::fprintf(stderr, "[Sprite2DResPrepare] shared_geometry created OK (%p)\n",
                     (void*)shared_unit_square_geometry);

        // Create shared sampler
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!sampler_manager)
        {
            std::fprintf(stderr, "[Sprite2DResPrepare] SamplerManager is null\n");
            return false;
        }

        if (!shared_sampler)
            shared_sampler = sampler_manager->CreateSampler();

        if (!shared_sampler)
            std::fprintf(stderr, "[Sprite2DResPrepare] CreateSampler FAILED\n");
        else
            std::fprintf(stderr, "[Sprite2DResPrepare] sampler created OK (%p)\n", (void*)shared_sampler);

        return shared_sampler != nullptr;
    }
} // namespace hgl::ecs
