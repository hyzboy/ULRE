#include <hgl/ecs/support/line/SimpleLineRenderSystem.h>
#include <hgl/ecs/support/line/SimpleLineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    SimpleLineRenderSystem::SimpleLineRenderSystem(const AnsiString& name)
        : RenderPipelineDrawSystem(name.ToStdString())
    {
        SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Line");   ///< 与 Line 组共开关（见 SimpleLineCollectSystem 注释）
    }

    RenderPipelineBase* SimpleLineRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(SimpleLineRenderPipeline::kName);
    }

    void SimpleLineRenderSystem::OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!pipeline)
        {
            GLogWarning("[SimpleLineRenderSystem] OnRender skipped: pipeline is null");
            return;
        }

        if (!cmd)
        {
            GLogWarning("[SimpleLineRenderSystem] OnRender skipped: cmd buffer is null");
            return;
        }

        pipeline->Render(cmd);
    }
}  // namespace hgl::ecs
