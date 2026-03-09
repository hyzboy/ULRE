#include <hgl/ecs/support/terrain/TerrainRenderPipelineGroup.h>
#include <hgl/ecs/support/terrain/TerrainRenderPipeline.h>
#include <hgl/ecs/support/terrain/TerrainCollectSystem.h>
#include <hgl/ecs/support/terrain/TerrainBuildSystem.h>
#include <hgl/ecs/support/terrain/TerrainRenderSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TerrainRenderPipelineGroup::TerrainRenderPipelineGroup()
        : RenderPipelineGroup("Terrain")
    {}

    bool TerrainRenderPipelineGroup::Initialize(ECSContext* context)
    {
        if (!context)
            return false;

        // 1. Create and register pipeline
        auto pipeline = std::make_unique<TerrainRenderPipeline>(context);
        context->RegisterRenderPipeline(TerrainRenderPipeline::kName, std::move(pipeline));

        // 2. Register systems in phase order:
        //      RenderCollect   → TerrainCollectSystem
        //      RenderBatch     → TerrainBuildSystem   (writes VAB + indirect buffer)
        //      RenderDrawSubmit→ TerrainRenderSystem   (one vkCmdDrawIndirect)
        context->RegisterRenderSystem<TerrainCollectSystem>();
        context->RegisterRenderSystem<TerrainBuildSystem>();
        context->RegisterRenderSystem<TerrainRenderSystem>();

        return true;
    }

    void TerrainRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

    std::unique_ptr<RenderPipelineBase> TerrainRenderPipelineGroup::CreatePipeline()
    {
        // Not used — pipeline created in Initialize() which has the context
        return nullptr;
    }

    void TerrainRenderPipelineGroup::RegisterSystems()
    {
        // Not used — systems registered to Context directly in Initialize()
    }

}  // namespace hgl::ecs
