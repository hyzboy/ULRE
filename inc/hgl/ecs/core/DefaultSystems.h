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

        bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt = nullptr);

        std::shared_ptr<InputSystem> RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
