#include<hgl/ecs/systems/render/LineStatsSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/LineCollectSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>

namespace hgl::ecs
{
    LineStatsSystem::LineStatsSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Unknown);
        SetExecutionOrder(ExecutionPhase::RenderStat);
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

        auto collect_system = world->GetSystem<LineCollectSystem>();
        auto render_system = world->GetSystem<LineRenderSystem>();
        if (!collect_system)
            return;

        const auto& stats = collect_system->GetStats();
        const uint32_t uploaded = render_system ? render_system->GetLastUploadedLineCount() : 0;

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
