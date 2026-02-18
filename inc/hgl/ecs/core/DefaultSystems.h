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
        class LineRenderSystem;

        struct DefaultEcsSystems
        {
            std::shared_ptr<InputSystem> input_system;
            std::shared_ptr<CameraSystem> camera_system;
            std::shared_ptr<LineRenderSystem> line_render_system;
        };

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
