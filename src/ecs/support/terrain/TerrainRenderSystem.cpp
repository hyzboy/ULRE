#include <hgl/ecs/support/terrain/TerrainRenderSystem.h>
#include <hgl/ecs/support/terrain/TerrainRenderPipeline.h>
#include <hgl/ecs/support/terrain/TerrainCollectSystem.h>
#include <hgl/ecs/support/terrain/TerrainBuildSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TerrainRenderSystem::TerrainRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Terrain");
        AddDependency<TerrainBuildSystem>();  // GPU buffers must be written before draw
    }

    RenderPipelineBase* TerrainRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(TerrainRenderPipeline::kName);
    }

    void TerrainRenderSystem::OnRender(RenderPipelineBase* pipeline,
                                       hgl::graph::RenderCmdBuffer* cmd)
    {
        pipeline->Render(cmd);
    }

}  // namespace hgl::ecs
