#include<hgl/ecs/systems/render/collect/SimpleLineStatsSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/line/SimpleLineRenderPipeline.h>
#include<hgl/common/DebugChannel.h>

namespace hgl::ecs
{
    SimpleLineStatsSystem::SimpleLineStatsSystem(const AnsiString& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderStat);
        SetRenderElementType("Line");   ///< 与 Line 组共开关（见 SimpleLineCollectSystem 注释）
    }

    void SimpleLineStatsSystem::Update(float /*deltaTime*/)
    {
        if (!ULRE_DEBUG_ON(Line))
            return;

        ++frame_counter;

        if (frame_counter % log_interval != 0)
            return;

        if (!world)
            return;

        auto* raw = world->GetRenderPipeline(SimpleLineRenderPipeline::kName);
        if (!raw)
            return;

        auto* pipeline = static_cast<SimpleLineRenderPipeline*>(raw);
        const auto& stats = pipeline->GetCollectStats();
        const uint32_t uploaded = pipeline->GetTotalLineCount();

        LogInfo("[SimpleLineStats] total=%u visible=%u skipped_empty=%u culled(vis=%u frustum=%u) ratio=%.2f uploaded_lines=%u",
                stats.total_components,
                stats.visible_components,
                stats.skipped_empty,
                stats.culled_by_visibility,
                stats.culled_by_frustum,
                stats.GetCullRatio(),
                uploaded);
    }
}
