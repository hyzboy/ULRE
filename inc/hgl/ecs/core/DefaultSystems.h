#pragma once

#include <memory>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }

    namespace ecs
    {
        class ECSContext;
        class InputSystem;
        class CameraSystem;
        class LineBoundsUpdateSystem;
        class LineCollectSystem;
        class LineRenderSystem;
        class LineStatsSystem;

        struct DefaultEcsSystems
        {
            std::shared_ptr<InputSystem> input_system;
            std::shared_ptr<CameraSystem> camera_system;
            std::shared_ptr<LineBoundsUpdateSystem> line_bounds_update_system;
            std::shared_ptr<LineCollectSystem> line_collect_system;
            std::shared_ptr<LineRenderSystem> line_render_system;
            std::shared_ptr<LineStatsSystem> line_stats_system;
        };

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
