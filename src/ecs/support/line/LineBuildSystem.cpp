#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

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
        if (!pipeline)
        {
            GLogWarning("[LineBuildSystem] OnBuild skipped: pipeline is null");
            return;
        }

        auto* line_pipeline = dynamic_cast<LineRenderPipeline*>(pipeline);
        if (line_pipeline)
        {
            const LineCollectStats& stats = line_pipeline->GetCollectStats();
            GLogInfo("[LineBuildSystem] Build begin: visible_components=%u total_components=%u pre_total_lines=%u",
                     stats.visible_components,
                     stats.total_components,
                     line_pipeline->GetTotalLineCount());
        }

        pipeline->RunBuild();

        if (line_pipeline)
        {
            GLogInfo("[LineBuildSystem] Build end: built_total_lines=%u",
                     line_pipeline->GetTotalLineCount());
        }
    }

}  // namespace hgl::ecs
