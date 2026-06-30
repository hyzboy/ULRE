#pragma once

#include <memory>
#include <string>

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
        class LineStatsSystem;

        struct DefaultEcsSystems
        {
            std::shared_ptr<InputSystem> input_system;
            std::shared_ptr<CameraSystem> camera_system;
            std::shared_ptr<LineBoundsUpdateSystem> line_bounds_update_system;
            std::shared_ptr<LineStatsSystem> line_stats_system;
        };

        bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt = nullptr);

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
