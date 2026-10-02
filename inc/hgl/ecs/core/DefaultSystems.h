#pragma once

#include <memory>
#include <string>
#include<hgl/ecs/support/ComponentTypeTable.h>

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
        /// 安装某系统组的全部系统（A2：按枚举，组名只在内部桥接一次性路径）
        bool EnsureSystemGroupSystems(ECSContext *ctx, SystemGroupID group, graph::IRenderTarget *default_rt = nullptr);

        DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt);
    }
}
