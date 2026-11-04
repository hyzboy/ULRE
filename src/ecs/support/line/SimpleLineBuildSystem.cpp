#include <hgl/ecs/support/line/SimpleLineBuildSystem.h>
#include <hgl/ecs/support/line/SimpleLineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    SimpleLineBuildSystem::SimpleLineBuildSystem(const AnsiString& name)
        : BuildSystem(name.ToStdString())
    {
        SetExecutionPhase(ExecutionPhase::RenderBatch);
        SetRenderElementType("Line");   ///< 与 Line 组共开关（见 SimpleLineCollectSystem 注释）
    }

    RenderPipelineBase* SimpleLineBuildSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(SimpleLineRenderPipeline::kName);
    }

    void SimpleLineBuildSystem::OnBuild(RenderPipelineBase* pipeline)
    {
        if (!pipeline)
        {
            GLogWarning("[SimpleLineBuildSystem] OnBuild skipped: pipeline is null");
            return;
        }

        pipeline->RunBuild();
    }

}  // namespace hgl::ecs
