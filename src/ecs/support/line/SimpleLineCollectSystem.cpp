#include <hgl/ecs/support/line/SimpleLineCollectSystem.h>
#include <hgl/ecs/support/line/SimpleLineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    SimpleLineCollectSystem::SimpleLineCollectSystem(const AnsiString& name)
        : CollectSystem(name.ToStdString())
    {
        SetExecutionPhase(ExecutionPhase::RenderCollect);
        // 组归属 = "Line"（kSystemGroupNames 之一）：SimpleLines 与 Lines 共用线条系统组，
        // 组开关（SetSystemGroupEnabled）只按组名点系统，元素类型不得自造新名。
        SetRenderElementType("Line");
    }

    RenderPipelineBase* SimpleLineCollectSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(SimpleLineRenderPipeline::kName);
    }

    void SimpleLineCollectSystem::OnCollect(RenderPipelineBase* pipeline)
    {
        if (!pipeline)
        {
            GLogWarning("[SimpleLineCollectSystem] OnCollect skipped: pipeline is null");
            return;
        }

        pipeline->RunCollect();
    }
}  // namespace hgl::ecs
