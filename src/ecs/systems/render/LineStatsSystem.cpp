#include<hgl/ecs/systems/render/LineStatsSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/line/LineCollectSystem.h>
#include<hgl/ecs/support/line/LineRenderSystem.h>
#include<hgl/ecs/support/line/LineRenderPipeline.h>

namespace hgl::ecs
{
    LineStatsSystem::LineStatsSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderStat);
        SetRenderElementType("Line");
        AddDependency<LineCollectSystem>();
        AddDependency<LineRenderSystem>();
    }

    void LineStatsSystem::Update(float /*deltaTime*/)
    {
        ++frame_counter;

        if (frame_counter % log_interval != 0)
            return;

        if (!world)
            return;

        auto* raw = world->GetRenderPipeline(LineRenderPipeline::kName);
        if (!raw)
            return;

        auto* pipeline = static_cast<LineRenderPipeline*>(raw);
        const auto& stats = pipeline->GetCollectStats();
        const uint32_t uploaded = pipeline->GetTotalLineCount();

        LogInfo("[LineStats] total=%u visible=%u culled(vis=%u frustum=%u hzb=%u) ratio=%.2f uploaded_lines=%u",
                stats.total_components,
                stats.visible_components,
                stats.culled_by_visibility,
                stats.culled_by_frustum,
                stats.culled_by_hzb,
                stats.GetCullRatio(),
                uploaded);
    }
}
