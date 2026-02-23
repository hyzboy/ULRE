#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    LineCollectSystem::LineCollectSystem(const std::string& name)
        : CollectSystem(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Line");
    }

    RenderPipelineBase* LineCollectSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(LineRenderPipeline::kName);
    }

    void LineCollectSystem::OnCollect(RenderPipelineBase* pipeline)
    {
        pipeline->RunCollect();
    }

}  // namespace hgl::ecs
