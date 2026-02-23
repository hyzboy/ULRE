#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    LineBuildSystem::LineBuildSystem(const std::string& name)
        : BuildSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Line");
        AddDependency<LineCollectSystem>();
    }

    RenderPipelineBase* LineBuildSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(LineRenderPipeline::kName);
    }

    void LineBuildSystem::OnBuild(RenderPipelineBase* pipeline)
    {
        pipeline->RunBuild();
    }

}  // namespace hgl::ecs
