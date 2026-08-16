#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    LineCollectSystem::LineCollectSystem(const std::string& name)
        : CollectSystem(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Line");
    }

    RenderPipelineBase* LineCollectSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(LineRenderPipeline::kName);
    }

    void LineCollectSystem::OnCollect(RenderPipelineBase* pipeline)
    {
        if (!pipeline)
        {
            GLogWarning("[LineCollectSystem] OnCollect skipped: pipeline is null");
            return;
        }

        pipeline->RunCollect();

        auto* line_pipeline = dynamic_cast<LineRenderPipeline*>(pipeline);
        if (!line_pipeline)
        {
            GLogWarning("[LineCollectSystem] OnCollect: pipeline is not LineRenderPipeline (ptr=%p)", pipeline);
            return;
        }

        const LineCollectStats& stats = line_pipeline->GetCollectStats();
        GLogInfo("[LineCollectSystem] Collect summary: total=%u visible=%u culled_visibility=%u culled_frustum=%u culled_hzb=%u cull_ratio=%.3f",
                 stats.total_components,
                 stats.visible_components,
                 stats.culled_by_visibility,
                 stats.culled_by_frustum,
                 stats.culled_by_hzb,
                 stats.GetCullRatio());
    }

}  // namespace hgl::ecs
