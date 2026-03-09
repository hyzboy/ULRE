#include <hgl/ecs/support/terrain/TerrainBuildSystem.h>
#include <hgl/ecs/support/terrain/TerrainRenderPipeline.h>
#include <hgl/ecs/support/terrain/TerrainCollectSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TerrainBuildSystem::TerrainBuildSystem(const std::string& name)
        : BuildSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Terrain");
        AddDependency<TerrainCollectSystem>();
    }

    RenderPipelineBase* TerrainBuildSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(TerrainRenderPipeline::kName);
    }

    void TerrainBuildSystem::OnBuild(RenderPipelineBase* pipeline)
    {
        pipeline->RunBuild();
    }

}  // namespace hgl::ecs
