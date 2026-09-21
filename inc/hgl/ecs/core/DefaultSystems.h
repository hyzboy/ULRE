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

        /// RegisterDefaultEcsSystems 的返回值：应用侧只消费 input_system
        ///（AppFramework 取其事件分发器挂事件链）；其余系统按 SystemGroup
        /// 自行管理，需要时经 ECSContext::GetSystem<T>() 查询。
        struct DefaultEcsSystems
        {
            std::shared_ptr<InputSystem> input_system;
        };

        void EnsureCoreEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt = nullptr);
        bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt = nullptr);

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
