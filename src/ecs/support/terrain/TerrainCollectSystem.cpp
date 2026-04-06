#include <hgl/ecs/support/terrain/TerrainCollectSystem.h>
#include <hgl/ecs/support/terrain/TerrainRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TerrainCollectSystem::TerrainCollectSystem(const std::string& name)
        : CollectSystem(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Terrain");
    }

    RenderPipelineBase* TerrainCollectSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(TerrainRenderPipeline::kName);
    }

    void TerrainCollectSystem::OnCollect(RenderPipelineBase* pipeline)
    {
        pipeline->RunCollect();
    }

}  // namespace hgl::ecs
